#ifndef APP_GSEC_H
#define APP_GSEC_H

/*
    gsec.h - link the flags to other parts of osmocombb
*/

struct vty *gsec_vty;
const char *gsec_number;
struct osmocom_ms *gsec_ms;
struct gsm48_rr_cd testcase_1_channel;

struct {
    int pid;
    int dcs;
    bool isWaiting;
    bool isActive;
} testcase_silentsms;

struct {
    int algo;
    bool isActive;
} testcase_encryption;

struct {
    int algo;
    bool isActive;
} testcase_zeroedimei;

struct{
    bool isActive;
    bool areWeCiphering;
} testcase_kraken;

struct gsm48_cip_mode_cmd *testcase_encryption_cipher_mode;

typedef struct list_stats{
    uint32_t tmsi;
    uint8_t kc[8];
    struct list_stats* next;
};

struct{
    struct list_stats* first_element;
    bool isActive;
} testaux_stats;



const char * flag_silentsms_frequencyhopping;
const char * flag_encryption;

// AUX - Report
int aux_checkstats_report();

// Test Case #1 
int execute_testcase_1();
void callback_testcase_1_step1();
void callback_testcase_1_step2();

// Test case #2
int execute_testcase_2();
void callback_testcase_2_step1(int check);
void callback_testcase_2_step2(int check);

// Test Case #3
int execute_testcase_3();
void callback_testcase_3_step1(int check);

// Test Case #4
int execute_testcase_4();

// Test Case #4
int execute_testcase_5();
int callback_test_5_step1(struct msgb *msg);

// GXK - Gsm eXploitation Kit

//Silent SMS
struct {
    // Current SMS Queue
    int current_message_number;

    // Max Messages
    int chain_messages;

    // Flag to Check if attack is running
    bool executing;

} attack_silentsms;


int execute_attack_1();
int execute_attack_1_callback();

#endif