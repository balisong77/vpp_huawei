#!/usr/bin/env python3

# Copyright (c) 2022-2024, Arm Limited.
#
# SPDX-License-Identifier: Apache-2.0

import os
import re
import sys
import argparse
import datetime

curYear = str(datetime.datetime.now().year)
begYear = '2022'

rstMsg = [
    '..\n',
    '  # Copyright (c) ' + curYear + ', Arm Limited.\n',
    '  #\n',
    '  # SPDX-License-Identifier: Apache-2.0\n',
    '\n'
]

shMsg = [
    '#!/usr/bin/env bash\n',
    '\n',
    '# Copyright (c) ' + curYear + ', Arm Limited.\n',
    '#\n',
    '# SPDX-License-Identifier: Apache-2.0\n',
    '\n'
]

pyMsg = [
    '#!/usr/bin/env python3\n',
    '\n',
    '# Copyright (c) ' + curYear + ', Arm Limited.\n',
    '#\n',
    '# SPDX-License-Identifier: Apache-2.0\n',
    '\n'
]

mkMsg = [
    '# Copyright (c) ' + curYear + ', Arm Limited.\n',
    '#\n',
    '# SPDX-License-Identifier: Apache-2.0\n',
    '\n'
]

cMsg = [
    '// Copyright (c) ' + curYear + ', Arm Limited.\n',
    '//\n',
    '// SPDX-License-Identifier: Apache-2.0\n',
    '\n'
]

suffixToMsg = {
    '.rst'  :   rstMsg,
    '.py'   :   pyMsg,
    '.sh'   :   shMsg,
    '.mk'   :   mkMsg,
    '.c'    :   cMsg,
    '.h'    :   cMsg,
    'Makefile'  :   mkMsg,
    'Dockerfile':   mkMsg,
}


def _argparsing():
    parser = argparse.ArgumentParser()
    parser.add_argument("--includes", nargs='+', metavar='DIR/FILE',
                        help="specify the directories/files")
    parser.add_argument("--excludes", nargs='+', metavar='DIR/FILE',
                        help="exclude the directories/files")
    parser.add_argument("-q", "--quiet", action="store_true",
                        help="avoid verbose message")
    parser.add_argument("--fix", action="store_true",
                        help="fix license identifier")
    args = parser.parse_args()
    return args


def getFiles(dir, suffixToMsg):
    absfiles = []
    for root, directory, files in os.walk(dir):
        for filename in files:
            if '.' in filename:
                name, extension = os.path.splitext(filename)
                if extension in suffixToMsg.keys():
                    absfiles.append(os.path.abspath(os.path.join(root,
                                    filename)))
            else:
                if filename.title() in suffixToMsg.keys():
                    absfiles.append(os.path.abspath(os.path.join(root,
                                    filename)))
    return absfiles


def parseFiles(dirfileList):
    absfiles = []
    for i in dirfileList:
        if os.path.isfile(i):
            absfiles.append(os.path.abspath(i))
        elif os.path.isdir(i):
            absfiles.extend(getFiles(i, suffixToMsg))
    return absfiles


def fixLicense(file, Msg):
    with open(file, 'r') as f:
        lines = f.readlines()
    insertPositionLocated = False
    # Replace existing License Identifier
    for i in range(len(lines)):
        if 'SPDX-License-Identifier' in lines[i]:
            for n in range(i+1, len(lines)):
                if '\n' == lines[n]:
                    lines = lines[n+1:]
                    insertPositionLocated = True
                    break
        if insertPositionLocated:
            break
    with open(file, 'w') as f:
        f.writelines(Msg)
        f.writelines(lines)


def checkLicense(args, file, Msg):
    with open(file, 'r') as f:
        lines = f.readlines()
    if len(Msg) > len(lines):
        print('  License-Identifier to be fixed: ', file)
        if (args.fix):
            if not args.quiet:
                print('    Fixing : ', file)
            fixLicense(file, Msg)
        return False
    for i in range(len(Msg)):
        if Msg[i] != lines[i]:
            needsToBeFixed = True
            if re.match(r'(  )?(#|//) Copyright \(c\) (.*), Arm Limited.', lines[i]):
                needsToBeFixed = False
                Years = re.findall("\d+", lines[i])
                if sorted(Years) != Years or Years[0] < begYear or Years[-1] > curYear:
                    needsToBeFixed = True
            if needsToBeFixed:
                print('  License-Identifier to be fixed: ', file)
                if (args.fix):
                    if not args.quiet:
                        print('    Fixing : ', file)
                    fixLicense(file, Msg)
                return False
    if not args.quiet:
        print('  License-Identifier OK: ', file)
    return True


def handleFiles(args, absinfiles, absexfiles, suffixToMsg):
    ToFixLicense = False
    for f in absinfiles:
        if f not in absexfiles:
            filename = os.path.basename(f)
            if '.' in filename:
                name, suffix = os.path.splitext(filename)
                key = suffix
            else:
                key = filename.title()
            if key in suffixToMsg.keys():
                s = checkLicense(args, f, suffixToMsg[key])
                if not s:
                    ToFixLicense = True
            else:
                print('  Unsupported filename: ', filename)
    return ToFixLicense


args = _argparsing()

absinfiles = parseFiles(args.includes)
absexfiles = parseFiles(args.excludes)

ToFixLicense = handleFiles(args, absinfiles, absexfiles, suffixToMsg)

if args.fix:
    sys.exit(0)

if ToFixLicense:
    sys.exit(1)
else:
    sys.exit(0)
