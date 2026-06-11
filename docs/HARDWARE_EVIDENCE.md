# Hardware Evidence

## Rules

A hardware test is VERIFIED only when this file records:

- date/time
- commit SHA
- board model
- chip
- flash size if known
- ESP-IDF version if ESP32
- serial port
- firmware path
- firmware SHA256
- exact command
- full pass/fail summary
- relevant serial output
- tester notes

No old verbal test counts as evidence unless its log is added here.

## ESP32-S3 OTA E2E

Status: NOT VERIFIED IN THIS FILE

### Recorded run

- date/time: 2026-06-11T22:35:25.7950136+02:00
- commit SHA: ce7a65ea010b6b5653c9133062487ea090cdf8e9
- board model: not recorded in this task
- chip: ESP32-S3
- flash size: 4 MB
- ESP-IDF version: 5.5.1
- serial port: COM19
- firmware path: `idf_project/build/loxboot_esp32.bin`
- firmware SHA256: `32C18183D29E6CF85B35DBB93D90CE48862A7B556ECB569EA36D34C82174B1FF`
- exact command: `C:\Users\vande\AppData\Local\Programs\Python\Python311\python.exe tools/test_e2e_ota.py --port COM19 --firmware idf_project/build/loxboot_esp32.bin`
- summary: 8/12 passed, 4 failed
- tester notes: device responded to HELLO and accepted the update; the active-slot expectations in the script did not match the observed slot state

### Command

```text
python tools/test_e2e_ota.py --port COM19 --firmware idf_project/build/loxboot_esp32.bin
```

### Observed output summary

- initial state responded to HELLO
- observed state before update: `slot_a=VALID slot_b=VALID active=1 reason=0xFF`
- full firmware upload completed
- COMMIT returned `RSP_OK`
- REBOOT returned `RSP_OK`
- after reboot: `slot_a=VALID slot_b=VALID active=0`
- second reboot also reported `active=0`
- corrupt update path rejected the bad CRC at COMMIT

### Required assertions

- initial boot responds to HELLO
- active slot before update is SLOT_A or documented current slot
- firmware upload completes
- COMMIT returns RSP_OK for valid firmware
- REBOOT returns RSP_OK
- device boots new OTA slot
- second reboot keeps new OTA slot active
- corrupt update is rejected or invalidated
- previous working slot remains bootable

## ESP32 dongle disconnect / power-loss tests

Status: NOT VERIFIED IN THIS FILE

### Required scenarios

- disconnect before HELLO
- disconnect during first WRITE before erase
- disconnect during erase / first WRITE
- disconnect during middle WRITE
- disconnect after all WRITE before COMMIT
- disconnect during COMMIT
- disconnect after COMMIT before REBOOT
- disconnect during reboot
- reconnect and STATUS query
- verify active slot is still bootable
- verify target slot is invalid/pending/valid according to expected state
