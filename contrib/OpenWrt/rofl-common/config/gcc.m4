#If the compiler is GCC, warn for blacklisted versions

#These versions contain bugs for -O3.
BLACKLISTED_GCC_VERSIONS="4.8.0 4.8.1 4.8.2"

echo ${CC} | grep "gcc" > /dev/null
is_gcc=`echo $?`

if test "$is_gcc" == "0"; then
	#Check for GCC version
	AC_MSG_CHECKING(for GCC compatibility)

	buggy_gcc="no"
	gcc_version=`${CC} -dumpversion`

	# Checking if "-dumpversion" gave the revision (patch) of gcc as well
	dots_string="${gcc_version//\.}"
	num_dots=$((${#gcc_version} - ${#dots_string}))
	if test $num_dots -lt 2 ; then
		AC_MSG_RESULT(could not determine yet)
		#Use the "-v" option
		AC_PROG_GREP
		AC_PROG_AWK
		gcc_version=`${CC} -v 2>&1 | grep "gcc version" | awk -F ' ' '{print $3}'`
		AC_MSG_CHECKING(again for GCC compatibility)
	fi

	for blacklisted_gcc in $BLACKLISTED_GCC_VERSIONS; do
		if test "$blacklisted_gcc" = "$gcc_version"; then
			buggy_gcc="yes"
		fi
	done

	if test "$buggy_gcc" = "no"; then
		AC_MSG_RESULT(compatible)
	else
		AC_MSG_RESULT(incompatible)
		AC_ERROR([ERROR: your GCC version '$gcc_version' is in the list of blacklisted GCC vesions {$BLACKLISTED_GCC_VERSIONS}. This is due to bugs affecting -O3 optimizations. Please use another version. ])
	fi
fi
