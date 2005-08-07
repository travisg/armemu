#ifndef __CONFIG_H
#define __CONFIG_H

int load_config(int argc, char **argv);

int add_config_key(const char *group, const char *key, const char *val);
const char *get_config_key_string(const char *group, const char *key, const char *default_val);


#endif