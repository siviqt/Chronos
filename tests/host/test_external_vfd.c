#include <assert.h>
#include <stdio.h>
#include "../../components/external_display/src/vfd.c"
static uint8_t   transactions[32][13];
static unsigned  lengths[32], writes, closed;
static esp_err_t open_error, write_error;
esp_err_t        external_vfd_transport_open(const external_config_t* c) {
    assert(c->vfd_address == 0x13);
    return open_error;
}
esp_err_t external_vfd_transport_write(const uint8_t* b, unsigned n) {
    assert(writes < 32 && n <= 13);
    memcpy(transactions[writes], b, n);
    lengths[writes++] = n;
    return write_error;
}
void external_vfd_transport_close(void) {
    ++closed;
}
static void check_format(unsigned source, const char* value, const char* detail, const char* expected) {
    external_frame_t f = {.source = source};
    snprintf(f.value, sizeof(f.value), "%s", value);
    snprintf(f.detail, sizeof(f.detail), "%s", detail);
    snprintf(f.title, sizeof(f.title), "Tokyo");
    char text[13];
    external_vfd_format(&f, text);
    assert(strlen(text) == 12 && !strcmp(text, expected));
    assert(!strcmp(f.value, value));  // The other displays retain their punctuation/tenths.
}
int main(void) {
    check_format(EXT_CLOCK, "21:19:35", "", "21 19 35    ");
    check_format(EXT_CLOCK, "09:19:35 PM", "", "09 19 35 PM ");
    check_format(EXT_WORLD, "21:19:35", "", "Tok 21 19 35");
    check_format(EXT_WORLD, "09:19:35 PM", "", "Tok 09 19 PM");
    check_format(EXT_COUNTDOWN, "04:27.3", "Running", "TM 04M27S  R");
    check_format(EXT_COUNTDOWN, "04:27.0", "Running", "TM 04M27S  R");
    check_format(EXT_COUNTDOWN, "00:00.0", "Complete", "TM 00M00S  C");
    check_format(EXT_COUNTDOWN, "60:00.0", "Ready", "TM 60M00S  I");
    check_format(EXT_STOPWATCH, "00:12.3", "Paused", "SW 00M12S  P");
    check_format(EXT_STOPWATCH, "123:45.6", "Stopped", "SW 123M45S S");
    check_format(EXT_STOPWATCH, "9999:59.9", "Running", "SW 9999M59SR");
    check_format(EXT_STOPWATCH, "9999m+", "Running", "SW 9999m+  R");
    check_format(EXT_CLOCK, "No data", "", "No data     ");
    check_format(EXT_WORLD, "No cities", "", "No cities   ");
    external_config_t c = EXTERNAL_CONFIG_DEFAULT;
    assert(external_vfd_driver.open(&c) == ESP_OK);
    assert(writes == 4);
    const uint8_t init[][2] = {{0, 1}, {3, 0}, {1, 0}, {6, 110}};
    for (unsigned i = 0; i < 4; ++i) assert(lengths[i] == 2 && !memcmp(transactions[i], init[i], 2));
    external_frame_t f = {.source = EXT_CLOCK, .value = "12:34:56"};
    assert(external_vfd_driver.present(&f) == ESP_OK);
    assert(lengths[4] == 13 && transactions[4][0] == 10 && !memcmp(transactions[4] + 1, "12 34 56    ", 12));
    assert(external_vfd_driver.present(&f) == ESP_OK && writes == 5);
    strcpy(f.value, "1");
    assert(external_vfd_driver.present(&f) == ESP_OK);
    assert(!memcmp(transactions[5] + 1, "1           ", 12));
    f = (external_frame_t){.source = EXT_COUNTDOWN, .value = "04:27.3", .detail = "Running"};
    assert(external_vfd_driver.present(&f) == ESP_OK);
    assert(!memcmp(transactions[writes - 1] + 1, "TM 04M27S  R", 12));
    unsigned before = writes;
    strcpy(f.value, "04:27.2");
    assert(external_vfd_driver.present(&f) == ESP_OK && writes == before);
    strcpy(f.value, "04:26.9");
    assert(external_vfd_driver.present(&f) == ESP_OK && writes == before + 1);
    write_error = ESP_ERR_TIMEOUT;
    strcpy(f.value, "2");
    assert(external_vfd_driver.present(&f) == ESP_ERR_TIMEOUT);
    external_vfd_driver.close();
    assert(closed == 1 && transactions[writes - 1][0] == 0 && transactions[writes - 1][1] == 0);
    open_error = ESP_ERR_NOT_FOUND;
    assert(external_vfd_driver.open(&c) == ESP_ERR_NOT_FOUND);
    external_vfd_driver.close();
    assert(closed == 1);
    open_error = ESP_OK;
    assert(external_vfd_driver.open(&c) == ESP_ERR_TIMEOUT);
    external_vfd_driver.close();
    assert(closed == 2);
    puts("VFD: exact NE-HCS12SS59T-R1 init, text register/padding, duplicate suppression and failures passed");
}
