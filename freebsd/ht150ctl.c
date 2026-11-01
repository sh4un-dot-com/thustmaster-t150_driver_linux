#include <sys/types.h>
#include <sys/sysctl.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *knobs[] = {
	"range",
	"gain",
	"autocenter",
	"enable_autocenter",
	"spring_level",
	"damper_level",
	"firmware_version",
	"max_range",
	NULL
};

static void
usage(void)
{
	fprintf(stderr,
	    "usage: ht150ctl [unit] [name [value]]\n"
	    "       ht150ctl [unit] test on|off\n"
	    "       ht150ctl [unit] preset [name]\n"
	    "       ht150ctl [unit] presets\n"
	    "\n"
	    "Sysctl knobs live at dev.ht150.<unit>.wheel.*\n"
	    "Feel presets: sim arcade street offroad rally drift truck\n");
	exit(1);
}

static int
sysctl_get(const char *name, int *out)
{
	uint64_t v = 0;
	size_t len = sizeof(v);

	if (sysctlbyname(name, &v, &len, NULL, 0) != 0)
		return (-1);
	*out = (int)v;
	return (0);
}

static int
sysctl_set(const char *name, int value)
{
	return (sysctlbyname(name, NULL, NULL, &value, sizeof(value)));
}

static int
sysctl_get_str(const char *name, char *out, size_t outlen)
{
	size_t len = outlen;

	if (sysctlbyname(name, out, &len, NULL, 0) != 0)
		return (-1);
	if (len < outlen)
		out[len] = '\0';
	else
		out[outlen - 1] = '\0';
	return (0);
}

static int
sysctl_set_str(const char *name, const char *value)
{
	return (sysctlbyname(name, NULL, NULL, value, strlen(value)));
}

int
main(int argc, char **argv)
{
	char path[128];
	int unit = 0;
	int i, val;
	const char *name;
	const char *value = NULL;

	if (argc >= 2 && argv[1][0] >= '0' && argv[1][0] <= '9' &&
	    argv[1][1] == '\0') {
		unit = argv[1][0] - '0';
		argv++;
		argc--;
	}

	if (argc == 1) {
		char preset[64];

		snprintf(path, sizeof(path), "dev.ht150.%d.wheel.preset", unit);
		if (sysctl_get_str(path, preset, sizeof(preset)) == 0)
			printf("preset=%s\n", preset);
		for (i = 0; knobs[i] != NULL; i++) {
			snprintf(path, sizeof(path),
			    "dev.ht150.%d.wheel.%s", unit, knobs[i]);
			if (sysctl_get(path, &val) == 0)
				printf("%s=%d\n", knobs[i], val);
		}
		return (0);
	}

	if (argc < 2 || argc > 3)
		usage();
	name = argv[1];
	if (argc == 3)
		value = argv[2];

	if (strcmp(name, "test") == 0) {
		int on;

		if (value == NULL)
			usage();
		on = (strcmp(value, "on") == 0 || strcmp(value, "1") == 0);
		snprintf(path, sizeof(path), "dev.ht150.%d.wheel.test_ff", unit);
		if (sysctl_set(path, on) != 0)
			err(1, "%s", path);
		printf("test_ff=%d\n", on);
		return (0);
	}

	if (strcmp(name, "presets") == 0) {
		char list[128];

		snprintf(path, sizeof(path), "dev.ht150.%d.wheel.presets", unit);
		if (sysctl_get_str(path, list, sizeof(list)) != 0)
			err(1, "%s", path);
		printf("%s\n", list);
		return (0);
	}

	if (strcmp(name, "preset") == 0) {
		char cur[64];

		snprintf(path, sizeof(path), "dev.ht150.%d.wheel.preset", unit);
		if (value == NULL) {
			if (sysctl_get_str(path, cur, sizeof(cur)) != 0)
				err(1, "%s", path);
			printf("preset=%s\n", cur);
			return (0);
		}
		if (sysctl_set_str(path, value) != 0)
			err(1, "%s", path);
		if (sysctl_get_str(path, cur, sizeof(cur)) == 0)
			printf("preset=%s\n", cur);
		return (0);
	}

	snprintf(path, sizeof(path), "dev.ht150.%d.wheel.%s", unit, name);
	if (value == NULL) {
		if (sysctl_get(path, &val) != 0)
			err(1, "%s", path);
		printf("%s=%d\n", name, val);
		return (0);
	}

	val = (int)strtol(value, NULL, 0);
	if (sysctl_set(path, val) != 0)
		err(1, "%s", path);
	if (sysctl_get(path, &val) == 0)
		printf("%s=%d\n", name, val);
	return (0);
}
