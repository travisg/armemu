/*
 * Copyright (c) 2005 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>

#include <config.h>

struct config_key {
	struct config_key *next;
	char *name;
	char *val;
};

struct config_group {
	struct config_group *next;
	char *name;
	struct config_key *keylist;	
};

static struct config_group *config_tree;

static struct config_group *find_config_group(const char *group)
{
	struct config_group *grp;
	
	for(grp = config_tree; grp != NULL; grp = grp->next) {
		if(strcasecmp(grp->name, group) == 0)
			return grp;
	}

	return NULL;
}

static struct config_key *find_config_key_in_group(struct config_group *group, const char *key)
{
	struct config_key *k;
	
	for(k = group->keylist; k != NULL; k = k->next) {
		if(strcasecmp(k->name, key) == 0)
			return k;
	}

	return NULL;
}

static struct config_key *find_config_key(const char *group, const char *key)
{
	struct config_group *grp;
	
	grp = find_config_group(group);
	if(grp == NULL)
		return NULL;
	return find_config_key_in_group(grp, key);
}

int add_config_key(const char *group, const char *key, const char *val)
{
	struct config_group *grp;
	struct config_key *k;

	printf("add_config_key: %s.%s = '%s'\n", group, key, val);

	/* find or create group */
	grp = find_config_group(group);
	if(grp == NULL) {
		/* create a new one */
		grp = malloc(sizeof(struct config_group));
		grp->name = strdup(group);
		grp->keylist = NULL;
		
		/* add it to the group list */
		grp->next = config_tree;
		config_tree = grp;	
	}	

	/* find or create key */
	k = find_config_key_in_group(grp, key);
	if(k == NULL) {
		/* create a new one */
		k = malloc(sizeof(struct config_key));
		k->name = strdup(key);
		k->val = strdup(val);
		
		/* add it to the group */
		k->next = grp->keylist;
		grp->keylist = k;
	} else {
		/* the key already exists, override it */
		if(k->val != NULL)
			free(k->val);
		k->val = strdup(val);
	}

	return 0;
}

const char *get_config_key_string(const char *group, const char *key, const char *default_val)
{
	struct config_key *k;
	
	k = find_config_key(group, key);
	if(k == NULL)
		return default_val;
		
	return k->val;
}

int get_config_key_bool(const char *group, const char *key, int default_val)
{
	struct config_key *k;
	
	k = find_config_key(group, key);
	if(k == NULL)
		return default_val;

	if (!strcasecmp(k->val, "true") || !strcasecmp(k->val, "yes") || atoi(k->val) != 0)
		return 1;
	else
		return 0;
}

static char *trim_string(char *str)
{
	char *s = str;
	char *end;

	/* remove leading spaces/tabs */
	while(isspace(*s))
		s++;
		
	/* check for null string */
	if(*s == '\0')
		return s; /* already zero length */
	
	/* remove trailing spaces and newlines */
	end = s + strlen(s) - 1;
	while(end != s && isspace(*end))
		end--;
	*(end + 1) = '\0';

	return s;
}

static int load_config_file(const char *filename)
{
	FILE *fp;
	char line_buf[1024];
	char group_buf[1024];
	char *curr_group;
	int line_num;

	printf("load_config_file: %s\n", filename);

	/* open the config file */
	fp = fopen(filename, "r");
	if(fp == NULL)
		return -1;

	/* not in a group yet */
	curr_group = group_buf;
	curr_group[0] = '\0';	
	
	line_num = -1;
	while(!feof(fp)) {
		char *s;
		char *g;
	
		/* read in a line */
		line_num++;
		s = fgets(line_buf, sizeof(line_buf), fp);
		if(s == NULL)
			break;

		/* remove leading and trailing spaces */
		s = trim_string(s);
		
		/* see if it's zero length */
		if(*s == '\0')
			continue;

//		printf("load_config_file: read line '%s'\n", s);

		/* see if it's a group, in '[group]' format */
		if(*s == '[') {
			s++;
			g = group_buf;
			while(*s != '\0' && *s != ']')
				*g++ = *s++;
			*g = 0;		
		
			/* trim the group buffer */
			curr_group = trim_string(group_buf);		
//			printf("load_config_file: group changed to '%s'\n", curr_group);
		} else if(*s == '#') {
			/* it's a comment, ignore */
			continue;
		} else {
			/* assume it's a name value pair, in 'name = val' format */
			char *name, *val;
			
			/* pick out the name */
			name = s;
			while(*s != '\0' && *s != '=')
				s++;
			if(*s == '\0') {
				printf("load_config_file: name with no value, line %d\n", line_num);
				continue;
			}
			*s++ = 0;
			
			/* value should be the rest of the line */
			val = s;
			
			/* trim the name and value pairs */
			name = trim_string(name);
			val = trim_string(val);			
			
//			printf("load_config_file: name '%s', value '%s'\n", name, val);

			/* see if we're outside of a group */
			if(curr_group[0] == '\0') {
				printf("load_config_file: name/value pair outside of group, line %d\n", line_num);
				continue;
			}

			/* add the name/value to the config tree */
			add_config_key(curr_group, name, val);
		}
	}


	fclose(fp);

	return 0;
}

int load_config(int argc, char **argv)
{
	/* initialize the config tree */
	config_tree = NULL;

	/* load from the config file */
	load_config_file(get_config_key_string("config", "file", "armemu.conf"));

	return 0;
}


