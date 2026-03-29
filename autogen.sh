#!/bin/sh
# Generate configure & friends for GIT users.

gnulib_url="git://git.sv.gnu.org/gnulib.git"
gnulib_hash="b75134c814c38876f04029ffc3fae4e90035dc34"

modules="
	canonicalize-lgpl
	futimens
	getdelim
	getline
	getopt-gnu
	glob
	isblank
	iswblank
	lstat
	mkstemps
	nl_langinfo
	regex
	sigaction
	snprintf-posix
	stdarg-h
	strcase
	strcasestr-simple
	strnlen
	sys_wait
	vsnprintf-posix
	wchar-h
	wctype-h
	wcwidth
"

# Make sure the local gnulib git repo is up-to-date.
if [ ! -d "gnulib" ]; then
	git clone --depth=2222 ${gnulib_url}
fi
cd gnulib >/dev/null || exit 1
curr_hash=$(git log -1 --format=%H)
if [ "${gnulib_hash}" != "${curr_hash}" ]; then
	echo "Pulling..."
	git pull
	git checkout --force ${gnulib_hash}
fi
cd .. >/dev/null || exit 1

rm -rf lib
echo "Gnulib-tool..."
./gnulib/gnulib-tool --import ${modules}
echo

echo "Autoreconf..."
autoreconf --install --symlink --force
echo "Done."
