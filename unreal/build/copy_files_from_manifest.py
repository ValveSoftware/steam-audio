# Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

import argparse
import os
import shutil
import xml.dom.minidom

def RemovePrefix(string, prefix):
    if string.startswith(prefix):
        return string[len(prefix):]
    else:
        return string

parser = argparse.ArgumentParser()
parser.add_argument('-s', '--srcroot', type=str)
parser.add_argument('-d', '--dstroot', type=str)
parser.add_argument('-m', '--manifest', type=str)

args = parser.parse_args()

if args.manifest is None or args.srcroot is None or args.dstroot is None:
    print('Invalid arguments.')
    exit(1)

srcRoot = os.path.normpath(args.srcroot)
dstRoot = os.path.normpath(args.dstroot)

dom = xml.dom.minidom.parse(args.manifest)
if dom.documentElement.tagName != 'BuildManifest':
    print('Invalid manifest file: ' + args.manifest)
    exit(1)

buildProductsElements = dom.documentElement.getElementsByTagName('BuildProducts')
if buildProductsElements is None or len(buildProductsElements) <= 0:
    print('Invalid manifest file: ' + args.manifest)
    exit(1)

buildProducts = buildProductsElements[0]

fileNameElements = buildProducts.getElementsByTagName('string')
if fileNameElements is None or len(fileNameElements) <= 0:
    print('Invalid manifest file: ' + args.manifest)
    exit(1)

for fileNameNode in fileNameElements:
    sourceFileName = fileNameNode.childNodes[0].data
    if not sourceFileName.startswith(srcRoot):
        print('WARNING: Build product not in srcroot, skipping: ' + sourceFileName)
        continue

    relativeFileName = RemovePrefix(sourceFileName, srcRoot).lstrip('/\\')

    destFileName = os.path.join(dstRoot, relativeFileName)
    print('-- Installing from Unreal manifest:' + destFileName)
    try:
        os.makedirs(os.path.dirname(destFileName))
    except:
        pass
    shutil.copy(sourceFileName, destFileName)
