/*
    GSEC Supporting Functions
    Duarte Monteiro
*/

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm48.h>
#include <osmocom/gsm/protocol/gsm_04_08.h>
#include <osmocom/gsm/protocol/gsm_08_58.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/signal.h>
#include <osmocom/core/prim.h>
#include <osmocom/crypt/auth.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/gsmtap.h>
#include <l1ctl_proto.h>

#include <osmocom/bb/common/osmocom_data.h>
#include <osmocom/bb/common/networks.h>
#include <osmocom/bb/common/l1ctl.h>
#include <osmocom/bb/mobile/mncc.h>
#include <osmocom/bb/mobile/vty.h>
#include <osmocom/bb/mobile/app_gsec.h>
#include <osmocom/bb/mobile/gsm480_ss.h>
#include <osmocom/bb/mobile/gsm411_sms.h>



/*
	Test cases predefined messages
*/

const char * flag_silentsms_frequencyhopping = "testcase_frequencyhopping&testcase_silentsms";
const char * flag_encryption = "testcase_encryption_A/";
const char * flag_zeroedimei = "testcase_zeroedimei";

/* 
	Auxiliary Functions 
*/

// Send SMS
int aux_sendsms(const char * testcase_text){
	struct osmocom_ms *ms = gsec_ms;
	const char *number = gsec_number;
	struct vty *vty = gsec_vty;
	
	struct gsm_settings *set;
	struct gsm_settings_abbrev *abbrev;
	char *sms_sca = NULL;
	
	//test_text = "*not#teste";
	set = &ms->settings;

	if (!set->sms_ptp) {
		vty_out(vty, "SMS not supported by this mobile, please enable "
			"SMS support%s", VTY_NEWLINE);
		return 0;
	}

	if (ms->subscr.sms_sca[0])
		sms_sca = ms->subscr.sms_sca;
	else if (set->sms_sca[0])
		sms_sca = set->sms_sca;

	if (!sms_sca) {
		vty_out(vty, "SMS sms-service-center not defined on SIM card, "
			"please define one at settings.%s", VTY_NEWLINE);
		return 0;
	}

	llist_for_each_entry(abbrev, &set->abbrev, list) {
		if (!strcmp(number, abbrev->abbrev)) {
			number = abbrev->number;
			vty_out(vty, "Using number '%s'%s", number,
				VTY_NEWLINE);
			break;
		}
	}

	sms_send(ms, sms_sca, number, testcase_text, 42);

	return 1;
}

// Send Service
int aux_sendservice(const char * testcase_service){
	ss_send(gsec_ms, testcase_service, 0);
	return 1;
}

//
// Reporting Module
//

// Report to the user the status
int aux_checkstats_report(){
	//Show stats to the user at the end of the report
	int counter;
	struct list_stats* current = testaux_stats.first_element;
	
	// Show how many times it changed TMSI and Kc
	
	//if list is empty
	if(current == NULL) {
	vty_out(gsec_vty, "[GSEC][*] No report available \r\n");
	  return 0;
	}

	counter = 0;
	do {
		vty_out(gsec_vty, "[GSEC][-] ** %d ** \r\n", counter);
		vty_out(gsec_vty, "[GSEC][-] TMSI: 0x%08x \r\n", current->tmsi);
		vty_out(gsec_vty, "[GSEC][-] KC:   0x");
		for (int i = 0; i < sizeof(&current->kc); i++)
			vty_out(gsec_vty, "%02x", current->kc[i]);
		vty_out(gsec_vty, "\r\n");
		counter++;
		current = current->next;
	} while(current->next != NULL);
	return 1;
}

// Check if KC was already used
bool checkifkcalreadyexists(uint8_t inputkey[8]){
	struct list_stats* current = testaux_stats.first_element;
	//if list is empty
	if(current == NULL) {
	  return false;
	}
	do {
		for(int i = 0; i < 8; i++){
			if(inputkey[i] != current->kc[i]){
				current = current->next;
			}
    	}
		return true;
	} while(current->next != NULL);

	return false;
	
}

// Select TMSI from list_stats
struct list_stats* selecttmsi(uint32_t tmsi) {

   //start from the first link
   struct list_stats* current = testaux_stats.first_element;

   //if list is empty
   if(current == NULL) {
      return NULL;
   }

   do{
      if(current->tmsi == tmsi) {
         return current;
      } else {
         current = current->next;
      }
	} while(current->next != NULL);
	
   return current;
}

// Insert new TMSI to the list
void insert_stats(uint32_t tmsi, uint8_t kc[8]) {
	//create a link
	struct list_stats *link = (struct list_stats*) malloc(sizeof(struct list_stats));
	
	link->tmsi = tmsi;
	memcpy(link->kc,kc, 8);
	

	link->next = testaux_stats.first_element;
	
	testaux_stats.first_element = link;
}

// Function to check if Kc and TMSI Changed
int aux_checkstats(){
	// Check if TMSI Changed, but also if KC changed only.
	struct gsm_subscriber *current_subscriber;
	if (!gsec_ms)
		return 0;
	
	current_subscriber = &gsec_ms->subscr;

	// Check if current TMSI is in the list
	if (selecttmsi(current_subscriber->tmsi) != NULL || checkifkcalreadyexists(current_subscriber->key) == false){
		// If not add to a list and warn user
		vty_out(gsec_vty, "[GSEC][+] TMSI changed to 0x%08x\r\n", current_subscriber->tmsi);  
		insert_stats(current_subscriber->tmsi, current_subscriber->key);
		return 1;
	}
	return 0;
}


/*
	Test Case #1
		- Support Silent SMS
		- Is Frequency Hopping enabled
*/

void callback_testcase_1_step1(){
	if (testcase_1_channel.h){
		vty_out(gsec_vty, "[GSEC][+] Frequency Hopping :( \r\n");  
		vty_out(gsec_vty, "[GSEC][+] MAIO     %u \r\n",testcase_1_channel.maio);  
		vty_out(gsec_vty, "[GSEC][+] HSN      %u \r\n",testcase_1_channel.hsn);
		vty_out(gsec_vty, "[GSEC][+] TSC      %u \r\n",testcase_1_channel.tsc);
		vty_out(gsec_vty, "[GSEC][+] CHAN #   %u \r\n",testcase_1_channel.chan_nr);
	} else { 
		vty_out(gsec_vty, "\r\n[GSEC][-] No Frequency Hopping  \r\n");
	}
	// Update stats
	aux_checkstats();
}

void callback_testcase_1_step2(){
	if (testcase_silentsms.isActive){
		vty_out(gsec_vty, "\r\n[GSEC][-] Network vulnerable to Silent SMS \r\n");
	} else {
		vty_out(gsec_vty, "\r\n[GSEC][+] Silent SMS is *not* Allowed \r\n");
	}
	// Update stats
	aux_checkstats();
}

int execute_testcase_1(){
	aux_checkstats();
	// Set Control for Frequency Hopping
	//struct testcase_isfrequencyhopping * testcase_isfrequencyhopping;
	

	// Set variables for Silent SMS
	testcase_silentsms.pid = 1;
	testcase_silentsms.dcs = 1;
	testcase_silentsms.isActive = false;
	testcase_silentsms.isWaiting = true;

	// Send SMS
	aux_sendsms(flag_silentsms_frequencyhopping);

	// Wait for the reception of message with the text testcase_text
	// Check if the message received contains the text testcase_text
	// if contains the testcase_text check of the DCS and PID (gsm411_sms.c)

	// Reset testcase for silent SMS
	testcase_silentsms.pid = 0;
	testcase_silentsms.dcs = 0;

	return 1;
}

/*
	Test Case #2
		- Support A5/ * Encryption
*/

void callback_testcase_2_step1(int check){
	if (check){
		vty_out(gsec_vty, "\r\n[GSEC][-] Ciphering mode COMPLETED algo: A5/%d \r\n", testcase_encryption.algo);
	} else {
		vty_out(gsec_vty, "\r\n[GSEC][+] Ciphering mode FAILED algo: A5/%d \r\n", testcase_encryption.algo);
	}
	// Update stats
	aux_checkstats();
}

void callback_testcase_2_step2(int check){
	if (check){
		vty_out(gsec_vty, "\r\n[GSEC][-] Network Rejected A5/%d \r\n", testcase_encryption.algo);
	} else {
		vty_out(gsec_vty, "\r\n[GSEC][+] Algorithm accepted: A5/%d \r\n", testcase_encryption.algo);
	}
	testcase_encryption.isActive = false;
	// Update stats
	aux_checkstats();
}

int execute_testcase_2(){
	// Check Stats
	aux_checkstats();

	// Get Settings
	struct gsm_settings *set;
	if (!gsec_ms)
		return 0;
	set = &gsec_ms->settings;

	// Set settings
	testcase_encryption.isActive = true;
	testcase_encryption.algo = 0;
	vty_out(gsec_vty, "[GSEC][*] Test for A5/%d \r\n",testcase_encryption.algo);
	
	// Set settings to no encryption
	set->a5_1 = 0;
	set->a5_2 = 1;
	set->a5_3 = 0;
	set->a5_4 = 0;
	set->a5_5 = 0;
	set->a5_6 = 0;
	set->a5_7 = 0;

	// Send SMS - [TO-DO] See if message is rejected / accepted and callback
	aux_sendsms(flag_encryption);
	
	

	// Instead of sending the sms we could get the classmark update and test allocation
	
	// Active flag for listening cipher mode packages
	// [Callback #1] If mode complete show results
	// [Callback #2] if Location Update Rejected -> NOK
	// [Callback #2] if Location update accepted -> OK
	
	// Turn off testing mode
	//testcase_encryption.isActive = false;

	return 1;
}

/*
	Test Case #3
		- Zeroed IMEI
*/

void callback_testcase_3_step1(int check){
	if (check){
		vty_out(gsec_vty, "\r\n[GSEC][-] Network accepts zeroed IMEI \r\n");
	} else {
		vty_out(gsec_vty, "\r\n[GSEC][+] Network denies zeroed IMEI \r\n");
	}
	testcase_zeroedimei.isActive = false;
}

int execute_testcase_3(){
	// Check Stats
	aux_checkstats();

	// Construct Zeroed IMEI
	char zeroed_imei[16] = "0000000000000000";	
	struct gsm_settings *set;

	// Check if we have MS
	if (!gsec_ms)
		return 0;
	set = &gsec_ms->settings;

	// Set IMEI to 0
	strcpy(set->imei,zeroed_imei);

	// Set Active Flag to True
	testcase_zeroedimei.isActive = true;

	// Send SMS ([TO-DO?] - Can we change to IMSI DETACH and MANUAL RE-SELECTION)
	aux_sendsms(flag_zeroedimei);
	return 1;
}

/*
	Test Case #4
		- DoS ()
*/

int execute_testcase_4(){
	return 0;
}

/*
	Test Case #5
		- Craken Requirements ()
*/

int aux_testcase_5_printmsg(struct msgb *msg){
	vty_out(gsec_vty, "[GSEC][*] Data Lenght: %d \r\n", msg->data_len);
	int i;
	
	if (msg->data != NULL){
		vty_out(gsec_vty,"Data: ");
		for(i=0; i<msg->data_len; i++){
			//printf("%d", msg->l3h[i]);
			//vty_out(gsec_vty, "%d", !!((msg->data[i] << j) & 0x80));
			vty_out(gsec_vty, " %02X", (unsigned int)(msg->data[i] & 0xFF));
		}
	}	
	vty_out(gsec_vty,"\r\n");
	
	return 0;
}


int execute_testcase_5_step1(struct msgb *msg, struct lapdm_entity *le, void *l3ctx){
	/*
	struct l1ctl_burst_ind *bi;
	struct msgb *msg = NULL;
	msg = input;
	
	aux_testcase_5(msg);
	*/

	struct gsm48_system_information_type_header *sih = msgb_l3(msg);
	struct gsmtap_hdr *gh = msgb_l1(msg);
	if (gh != NULL){
		vty_out(gsec_vty, "[GSEC][*] Frame number: %d \r\n", gh->frame_number);
	}
	
	
	switch (sih->system_information) {
		case GSM48_MT_RR_SYSINFO_1:
			//vty_out(gsec_vty, "[GSEC][*][%d] New system Information 1 \r\n", );			
			//aux_testcase_5_printmsg(msg);
			break;
		case GSM48_MT_RR_SYSINFO_4:
			vty_out(gsec_vty, "[GSEC][*] New system Information 4 \r\n");
			aux_testcase_5_printmsg(msg);
			break;
		case GSM48_MT_RR_SYSINFO_5:
			vty_out(gsec_vty, "[GSEC][*] New system Information 5 \r\n");
			aux_testcase_5_printmsg(msg);
			break;
		default:
			break;
	}
	
	msgb_free(msg);
	return 0;
}

/*
	Testing For Kraken requirements
*/
int execute_testcase_5(){

	struct gsm_subscriber subs = gsec_ms->subscr;
	if( subs.key != NULL){
		testcase_kraken.areWeCiphering = true;		
	}
	
	vty_out(gsec_vty, "[GSEC][*] Testing for Kraken Requirements \r\n");
	lapdm_channel_set_l3(&gsec_ms->lapdm_channel, &execute_testcase_5_step1, gsec_ms);
	return 0;
}

/**********************************************
 * Type-0 SMS Attack - Proof-of-concept
 * 
***********************************************/

int execute_attack_1(){
	attack_silentsms.executing = true;
	attack_silentsms.current_message_number = 0;

	// Set variables for Silent SMS
	testcase_silentsms.pid = 1;
	testcase_silentsms.dcs = 1;
	
	// Send First SMS
	aux_sendsms(flag_silentsms_frequencyhopping);
	attack_silentsms.current_message_number += 1;

}

int execute_attack_1_callback(){
	// There are still SMS to be sent
	if (attack_silentsms.current_message_number <= attack_silentsms.chain_messages){
		vty_out(gsec_vty, "[GXK][-] Sending Silent-SMS #%d\r\n", attack_silentsms.current_message_number);
		// Send SMS
		aux_sendsms(flag_silentsms_frequencyhopping);
		// Increment the count
		attack_silentsms.current_message_number +=1;
	} else {
		vty_out(gsec_vty, "[GXK][-] Attack Finished \r\n");
	}
}