/*
 * VDE - vde_vxlan Network emulator for vde
 * Copyright (C) 2014 Renzo Davoli, Alessandro Ghedini VirtualSquare
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdarg.h>

#include "log.h"

static const char *prog = "vde_vxlan";

int logok = 0;
int debug = 0;

void printlog(int priority, const char *format, ...) {
	va_list arg;

	if (!debug && priority == LOG_DEBUG)
		return;

	va_start(arg, format);
	if (logok)
		vsyslog(priority, format, arg);
	else {
		fprintf(stderr, "%s: ", prog);
		vfprintf(stderr, format, arg);
		fprintf(stderr, "\n");
	}
	va_end(arg);
}

void printoutc(FILE *f, const char *format, ...) {
	va_list arg;

	va_start (arg, format);
	if (f) {
		vfprintf(f,format,arg);
		fprintf(f,"\n");
	} else
		printlog(LOG_INFO,format,arg);
	va_end(arg);
}
