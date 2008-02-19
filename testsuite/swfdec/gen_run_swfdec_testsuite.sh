#"
# Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#!/bin/sh -x


if [ -z "$3" ]; then
	echo "Usage: `basename %0` <gnash_builddir> <gnash_srcdir> <swfdec_tracedir> [<startingcharacters>]" >&2
	exit 1
fi

BUILDDIR="$1"
SRCDIR="$2"
SWFDECTRACEDIR="$3"
STARTPATTERN="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
if [ -n "$4" ]; then
	STARTPATTERN="$4"
fi
NOW=`date`

GPROCESSOR="${BUILDDIR}/utilities/gprocessor"
GNASHRC="${BUILDDIR}/testsuite/gnashrc"
GNASH_PLAYER="${BUILDDIR}/gui/gnash"
SWFDEC_GNASH_TESTER="${SRCDIR}/swfdec_gnash_tester"

cat<<EOF 
#!/bin/sh

# Generated by gen_run_swfdec_testsuite.sh on
# $NOW

export GNASHRC=${GNASHRC}
export GPROCESSOR=${GPROCESSOR}
EXPECTPASS=${SRCDIR}/PASSING
REALTIME=${SRCDIR}/REALTIME

for test in \`ls ${SWFDECTRACEDIR}/[$STARTPATTERN]*.swf\`; do
	testname=\`basename \${test}\`
	md5=\`md5sum \${test} | cut -d' ' -f1\`
	testid="\${testname}:\${md5}"

	if [ -f \${test}.act ]; then
		echo "NOTE: skipping \${testname} (unsupported user interaction)"
		echo "UNTESTED: \${testid} (requires unsupported user interaction)"
		continue
	fi

	expectpass=no
	if grep -q "^\${testid}\$" \${EXPECTPASS}; then
		expectpass="yes"
	fi
	flags=
	if grep -q "^\${testname}\$" \${REALTIME}; then
		flags="-d -1"
		echo "NOTE: running \${testname} (realtime - expect pass: \${expectpass})"
	else
		echo "NOTE: running \${testname} (expect pass: \${expectpass})"
	fi
	if ${SWFDEC_GNASH_TESTER} \${test} \${flags} > \${testname}.log; then
		if [ "\${expectpass}" = "yes" ]; then
			echo "PASSED: \${testid}"
		else
			echo "XPASSED: \${testid}"
		fi	
	else
		if [ "\${expectpass}" = "yes" ]; then
			echo "FAILED: \${testid} (traces in \${testname}.trace-gnash, log in \${testname}.log)"
		else
			echo "XFAILED: \${testid} (traces in \${testname}.trace-gnash, log in \${testname}.log)"
		fi
	fi
done

EOF

