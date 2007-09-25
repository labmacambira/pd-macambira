#!/bin/sh

LIBS="libdir Gem cyclone zexy creb cxc ggee iemlib list-abs mapping markex maxlib memento mjlib motex oscx pddp pdogg pixeltango pmpd rradical sigpack smlib toxy unauthorized vbap pan freeverb hcs jmmmp ext13 hardware deprecated flatspace iem_anything pdp pidip flib ekext"

GNULINUX_FONTPATH="/usr/share/pd /var/lib/defoma/x-ttcidfont-conf.d/dirs/TrueType"
MACOSX_FONTPATH="/Library/Pd /System/Library/Fonts /Library/Fonts ~/Library/Fonts /usr/X11R6/lib/X11/fonts/TTF /System/Library/Frameworks/JavaVM.framework/Versions/1.3.1/Home/lib/fonts /sw/lib/X11/fonts/msttf /sw/lib/X11/fonts/intl/TrueType /sw/lib/X11/fonts/applettf"
# TODO this needs to be changed to add to a user dir like %USERPROFILE% (like /Library/Pd)
WINDOWS_FONTPATH="hex(2):25,00,53,00,79,00,73,00,74,00,65,00,6d,00,52,00,6f,00,6f,00,74,00,25,00,2f,00,46,00,6f,00,6e,00,74,00,73,00,00,00"

SCRIPT_DIR=`echo $0 | sed 's|\(.*\)/.*$|\1|'`
ROOT_DIR=${SCRIPT_DIR}/../packages

GNULINUX_FILE=${ROOT_DIR}/linux_make/default.pdsettings
MACOSX_FILE=${ROOT_DIR}/darwin_app/org.puredata.pd.plist
WINDOWS_FILE=${ROOT_DIR}/win32_inno/pd-settings.reg
WINDOWS_INNO_FILE=${ROOT_DIR}/win32_inno/pd-inno_registry.reg

GNULINUX_HEADER='standardpath: 1\nverbose: 0\ndefeatrt: 0\nflags: -helppath /usr/share/pd'


MACOSX_HEADER='<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n<plist version="1.0">\n<dict>\n\t<key>defeatrt</key>\n\t<string>0</string>\n\t<key>flags</key>\n\t<string>-helppath /Library/Pd</string>'
MACOSX_FOOTER='</dict>\n
</plist>\n'


WINDOWS_HEADER='Windows Registry Editor Version 5.00\n\n[HKEY_LOCAL_MACHINE\SOFTWARE\Pd]'


# the file associations should be added here
WINDOWS_INNO_HEADER=''


echo -e $GNULINUX_HEADER > $GNULINUX_FILE
echo -e $MACOSX_HEADER > $MACOSX_FILE
echo -e $WINDOWS_HEADER > $WINDOWS_FILE
echo -e $WINDOWS_INNO_HEADER > $WINDOWS_INNO_FILE


# GNU/Linux -------------------------------------------------------------------#
print_gnulinux ()
{
	 echo "loadlib$1: $2" >> $GNULINUX_FILE
}

print_gnulinux_fontpath ()
{
	 i=0
	 for fontpath in $GNULINUX_FONTPATH; do
		  ((++i)) 
		  echo "path${i}: ${fontpath}" >> $GNULINUX_FILE
	 done
}

print_gnulinux_footer ()
{
	 echo "nloadlib: $1" >> $GNULINUX_FILE
}

# Mac OS X --------------------------------------------------------------------#
print_macosx () 
{
	 echo -e "\t<key>loadlib$1</key>" >> $MACOSX_FILE
	 echo -e "\t<string>$2</string>" >> $MACOSX_FILE
}

print_macosx_fontpath ()
{
	 i=0
	 for fontpath in $MACOSX_FONTPATH; do
		  ((++i)) 
		  echo -e "\t<key>path${i}</key>" >> $MACOSX_FILE
		  echo -e "\t<string>${fontpath}</string>" >> $MACOSX_FILE
	 done
}

# Windows ---------------------------------------------------------------------#
print_windows ()
{
	 echo "\"loadlib$1\"=\"$2\"" >> $WINDOWS_FILE
	 echo "Root: HKLM; SubKey: SOFTWARE\Pd; ValueType: string; ValueName: loadlib$1; ValueData: $2; Tasks: libs"  >> $WINDOWS_INNO_FILE
}

print_windows_fontpath ()
{
	 j=0
	 for fontpath in $WINDOWS_FONTPATH; do
		  ((++j)) 
		  echo "\"path${j}\"=${fontpath}" >> $WINDOWS_FILE
		  echo "\"path${j}\"=${fontpath}" >> $WINDOWS_INNO_FILE
	 done
}

print_windows_delete ()
{
	 echo "\"${1}${2}\"=-" >> $WINDOWS_FILE
	 echo "Root: HKLM; SubKey: SOFTWARE\Pd; ValueType: none; ValueName: ${1}${2}; Flags: deletevalue; Tasks: libs"  >> $WINDOWS_INNO_FILE
}

#==============================================================================#

echo "Running for GNU/Linux and Darwin:"
i=0
for lib in $LIBS; do
	 ((++i)) 
	 echo -n "$lib "
	 print_gnulinux $i $lib
	 print_macosx $i $lib
done
echo " "

# the .pdsettings file needs a terminator with the lib count
linux_end_count=$i

# run separately so some libs can be excluded on Windows
echo "Running for Windows:"
i=0
for lib in $LIBS; do
	 case "$lib" in
		  pdp) echo -n "(ignoring $lib on Windows) " ;;
		  pidip) echo -n "(ignoring $lib on Windows) " ;;
		  *)
				echo -n "$lib "
				((++i)) 
				print_windows $i $lib
				;;
	 esac
done
echo " "

# print lines to delete existing loadlib flags
echo "; delete any previous loadlib flags" >> $WINDOWS_FILE
while [ $i -lt 100 ]; do
	 ((++i)) 
	 print_windows_delete loadlib $i
done

print_windows_fontpath

# print lines to delete existing path flags
i=1
echo "; delete all existing path flags" >> $WINDOWS_FILE
while [ $i -lt 100 ]; do
	 ((++i)) 
	 print_windows_delete path $i
done

# the .pd-settings file needs an end tag for the loadlib statements
print_gnulinux_fontpath
print_gnulinux_footer $linux_end_count

print_macosx_fontpath
echo -e $MACOSX_FOOTER >> $MACOSX_FILE

