/*
 * Support for displaying firmware information managed by dboot
 *
 * Copyright (C) 2010 DoréDevelopment ApS
 *
 * Author: Esben Haabendal <eha@doredevelopment.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <stdio.h>

#include "dboot.h"

int
list_bl(void)
{
	char buf[80];
	int err;

	err = get_bl(buf);
	if (err)
		return err;

	printf("%s\n", buf);
	return 0;
}

int
list_os_a(void)
{
	char buf[80];
	int err;

	err = get_os_a(buf);
	if (err)
		return err;

	printf("%s\n", buf);
	return 0;
}

int
list_os_b(void)
{
	char buf[80];
	int err;

	err = get_os_b(buf);
	if (err)
		return err;

	printf("%s\n", buf);
	return 0;
}

int
list_os(void)
{
	int err;

	err = list_os_a();
	if (err)
		return err;

	err = list_os_b();
	if (err)
		return err;

	return 0;
}

int
list_all(void)
{
	int err;

	err = list_bl();
	if (err)
		return err;

	err = list_os();
	if (err)
		return err;

	return 0;
}
