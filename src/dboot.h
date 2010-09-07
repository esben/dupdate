#include <stdint.h>

/* backend API */
int dboot_backend_init(void);
void dboot_backend_cleanup(void);
int get_status_word(uint16_t *);
int set_status_word(uint16_t);
int get_bl(char *);
int get_os_a(char *);
int get_os_b(char *);
int set_bl(const char *);
int set_os_a(const char *);
int set_os_b(const char *);

