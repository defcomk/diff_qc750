/*
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This software is the confidential and proprietary information of
 * Trusted Logic S.A. ("Confidential Information"). You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered
 * into with Trusted Logic S.A.
 *
 * TRUSTED LOGIC S.A. MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE
 * SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT. TRUSTED LOGIC S.A. SHALL
 * NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING,
 * MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
 */
#ifndef __SCHANNEL6_LOGINS_H__
#define __SCHANNEL6_LOGINS_H__

#define SCX_LOGIN_PUBLIC              0x00000000
#define SCX_LOGIN_USER                0x00000001
#define SCX_LOGIN_GROUP               0x00000002
#define SCX_LOGIN_APPLICATION         0x00000004
#define SCX_LOGIN_APPLICATION_USER    0x00000005
#define SCX_LOGIN_APPLICATION_GROUP   0x00000006
#define SCX_LOGIN_AUTHENTICATION      0x80000000
#define SCX_LOGIN_PRIVILEGED          0x80000002

/* Login variants */

#define SCX_LOGIN_VARIANT(mainType, os, variant) \
   ((mainType) | (1 << 27) | ((os) << 16) | ((variant) << 8))

#define SCX_LOGIN_GET_MAIN_TYPE(type) ((type) & ~SCX_LOGIN_VARIANT(0, 0xFF, 0xFF))

#define SCX_LOGIN_OS_ANY       0x00
#define SCX_LOGIN_OS_LINUX     0x01
#define SCX_LOGIN_OS_WINMOBILE 0x02
#define SCX_LOGIN_OS_SYMBIAN   0x03
#define SCX_LOGIN_OS_ANDROID   0x04

/* OS-independent variants */
#define SCX_LOGIN_USER_NONE        SCX_LOGIN_VARIANT(SCX_LOGIN_USER, SCX_LOGIN_OS_ANY, 0xFF)
#define SCX_LOGIN_GROUP_NONE       SCX_LOGIN_VARIANT(SCX_LOGIN_GROUP, SCX_LOGIN_OS_ANY, 0xFF)
#define SCX_LOGIN_APPLICATION_USER_NONE \
               SCX_LOGIN_VARIANT(SCX_LOGIN_APPLICATION_USER, SCX_LOGIN_OS_ANY, 0xFF)
#define SCX_LOGIN_AUTHENTICATION_BINARY_SHA1_HASH \
               SCX_LOGIN_VARIANT(SCX_LOGIN_AUTHENTICATION, SCX_LOGIN_OS_ANY, 0x01)
#define SCX_LOGIN_PRIVILEGED_KERNEL \
               SCX_LOGIN_VARIANT(SCX_LOGIN_PRIVILEGED, SCX_LOGIN_OS_ANY, 0x01)

/* Linux variants */
#define SCX_LOGIN_USER_LINUX_EUID     SCX_LOGIN_VARIANT(SCX_LOGIN_USER, SCX_LOGIN_OS_LINUX, 0x01)
#define SCX_LOGIN_GROUP_LINUX_GID     SCX_LOGIN_VARIANT(SCX_LOGIN_GROUP, SCX_LOGIN_OS_LINUX, 0x01)
#define SCX_LOGIN_APPLICATION_LINUX_PATH_SHA1_HASH \
               SCX_LOGIN_VARIANT(SCX_LOGIN_APPLICATION, SCX_LOGIN_OS_LINUX, 0x01)
#define SCX_LOGIN_APPLICATION_USER_LINUX_PATH_EUID_SHA1_HASH \
               SCX_LOGIN_VARIANT(SCX_LOGIN_APPLICATION_USER, SCX_LOGIN_OS_LINUX, 0x01)
#define SCX_LOGIN_APPLICATION_GROUP_LINUX_PATH_GID_SHA1_HASH \
               SCX_LOGIN_VARIANT(SCX_LOGIN_APPLICATION_GROUP, SCX_LOGIN_OS_LINUX, 0x01)

/* Android variants */
#define SCX_LOGIN_USER_ANDROID_EUID   SCX_LOGIN_VARIANT(SCX_LOGIN_USER, SCX_LOGIN_OS_ANDROID, 0x01)
#define SCX_LOGIN_GROUP_ANDROID_GID   SCX_LOGIN_VARIANT(SCX_LOGIN_GROUP, SCX_LOGIN_OS_ANDROID, 0x01)
#define SCX_LOGIN_APPLICATION_ANDROID_UID \
               SCX_LOGIN_VARIANT(SCX_LOGIN_APPLICATION, SCX_LOGIN_OS_ANDROID, 0x01)
#define SCX_LOGIN_APPLICATION_USER_ANDROID_UID_EUID \
               SCX_LOGIN_VARIANT(SCX_LOGIN_APPLICATION_USER, SCX_LOGIN_OS_ANDROID, 0x01)
#define SCX_LOGIN_APPLICATION_GROUP_ANDROID_UID_GID \
               SCX_LOGIN_VARIANT(SCX_LOGIN_APPLICATION_GROUP, SCX_LOGIN_OS_ANDROID, 0x01)

/* Symbian variants */
#define SCX_LOGIN_APPLICATION_SYMBIAN_UIDS \
               SCX_LOGIN_VARIANT(SCX_LOGIN_APPLICATION, SCX_LOGIN_OS_SYMBIAN, 0x01)
#define SCX_LOGIN_APPLICATION_USER_SYMBIAN_UIDS \
               SCX_LOGIN_VARIANT(SCX_LOGIN_APPLICATION_USER, SCX_LOGIN_OS_SYMBIAN, 0x01)
#define SCX_LOGIN_APPLICATION_GROUP_SYMBIAN_UIDS \
               SCX_LOGIN_VARIANT(SCX_LOGIN_APPLICATION_GROUP, SCX_LOGIN_OS_SYMBIAN, 0x01)
#define SCX_LOGIN_AUTHENTICATION_SYMBIAN_UIDS \
               SCX_LOGIN_VARIANT(SCX_LOGIN_AUTHENTICATION, SCX_LOGIN_OS_SYMBIAN, 0x01)


#endif /* __SCHANNEL6_LOGINS_H__ */