#
# Copyright (c) 2005-2008 Trusted Logic S.A.
# All Rights Reserved.
#
# This software is the confidential and proprietary information of
# Trusted Logic S.A. ("Confidential Information"). You shall not
# disclose such Confidential Information and shall use it only in
# accordance with the terms of the license agreement you entered
# into with Trusted Logic S.A.
#
# TRUSTED LOGIC S.A. MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE
# SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT. TRUSTED LOGIC S.A. SHALL
# NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING,
# MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
#

from subprocess import *
import os
import atexit
import sys
import codecs 
import re
import stat

scriptDir = sys.path[0]

#
# Find a suitable openssl binary
#
def FindOpenSSL():

	openssl = None

	if sys.platform == "win32":
	   opensslSuffix = os.path.join("openssl", "ix86_win32", "bin", "openssl.exe")
	elif sys.platform == "linux2":
	   opensslSuffix = os.path.join("openssl", "ix86_linux", "bin", "openssl")
	else:
	   raise Exception("Unsupported sys.platform")

	def generateOpenSSLCandidates():
	   dir = scriptDir
	   while dir != None:            
		  yield os.path.join(dir, opensslSuffix)
		  yield os.path.join(dir, "externals", opensslSuffix)
		  yield os.path.join(dir, "unsupported", opensslSuffix)
		  parent = os.path.dirname(dir)
		  if dir == parent:
			 break
		  dir = parent

	for f in generateOpenSSLCandidates():
	   if os.path.exists(f):   
	      if not os.access(f, os.X_OK):
	         print "Warning: this copy of openssl is not executable"
	         if sys.platform == "linux2":
	            os.chmod(f, stat.S_IMODE(os.stat(f).st_mode) | stat.S_IXUSR)
	            openssl = f
	      else:
	         openssl = f
	      break

	if openssl is None:
	   print "Warning: Can't find a suitable copy of openssl"
	   openssl="openssl"
	   
	return openssl

#
# Utility function to launch a program
#
STDOUT=-1
RETURN=-2
def Exec(cmd, input=None, output=RETURN):   
   if input is None:
      stdin = None
   else:
      stdin = PIPE
   if output == STDOUT:
      stdout = None
   elif output == RETURN:
      stdout = PIPE
   else:
      stdout = output 
   p = Popen(cmd, stdin=stdin, stdout=stdout)   
   output = p.communicate(input=input)[0]     
   errorcode = p.wait()
   if errorcode != 0:
      raise Exception("%s returned error %d" % (cmd, errorcode))
   return output

#
# Utility function to parse a source manifest file
# stripping the comments and handing the Unicode stuff
#
manifestLinePattern = re.compile(u"^([A-Za-z0-9][A-Za-z0-9_.\-]+)[ \t]*:[ \t]*(.*)[ \t]*$", re.UNICODE)
def ReadManifest(manifest_file):
   # read the entire manifest as a Unicode string
   manifest = codecs.open(manifest_file, "r", "utf-8").read()
   # Strip-off leading BOM, if present
   if manifest[0] == u'\ufeff':
      manifest = manifest[1:]
   # Check if there is an embedded NUL character
   if manifest.find(u'\u0000') >= 0:
      raise Exception(manifest_file + ": there is an NUL character in this manifest" )
   lineNum = 0
   properties = {}
   for line in manifest.splitlines():
      lineNum = lineNum + 1
      if len(line) == 0 or line[0] == u'#':
         # this is a blank line or a comment
         continue
      else:
         match = manifestLinePattern.match(line)
         if match == None:
            raise Exception("%s, line %d: invalid syntax" % (manifest_file, lineNum))
         else:
            key = match.group(1)
            value = match.group(2)
            if properties.has_key(key):
               raise Exception("%s, line %d: duplicate property %s" % (manifest_file, lineNum, key))
            properties[key] = value
   return properties   

   