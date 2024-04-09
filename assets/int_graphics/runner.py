#!env python

import os
import re
import sys
import array
import subprocess
import collections
import tempfile
import argparse

prolog = """This script allows to run the int_graphics test with
			multiple shaders from single directory (-tgt parameter)
			at the time. The shaders stand a group if they have
			the same name, not an extension, so a.vert and a.frag
			would belong to the group 'a' whereas b.vert, b.geom
			and b.frag to the group 'b'. Only extensions .vert,
			.tese, .tesc, .geom and .frag are recognized and single
			group must have at least vertex and fragment shaders.
			Remaining the shaders are optional however if they exist
			then these will be added to the graphics pipeline.
			You can also specify -mask parameter. If it is not empty
			then it is treated as normal regular expression that is
			passed directly to this script, thus be aware how your
			command interpreter treats the characters inside the
			quoted string. Sometimes it is a good idea to specify
			other temporary directory than system one. For an
			example you are working on ram disk. In that case you
			can provide it by specifying the -tmp parameter. The
			only one parameter which is mandatory is the VTF
			application which will process the shaders. It is
			specified by -app parameter and it can be a folder that
			contains it or just a path to VTF.
			All unrecognized parameter will be passed directly to
			the VTF app, so please be familiar with parameters which
			the int_graphics test uses."""

parser = argparse.ArgumentParser(description = prolog,
		epilog="All unrecognized parameters will treat as if they were passed to in_graphics test")
parser.add_argument("-app", dest="app", type=str, help="Define app executable path", default=".")
parser.add_argument("-tmp",
					dest="tmp", type=str,
					help='Define temporary dir, default is "{}"'.format(tempfile.gettempdir()),
					default=tempfile.gettempdir())
parser.add_argument("-tgt", dest="tgt", type=str, help="Define a folder path with the shaders", default=".")
parser.add_argument("-mask", dest="mask", type=str, help="Define regex flter for test group names", default="")
parser.add_argument("-v", dest="verbose", action="store_true", help="Be more verbosity")
parser.add_argument("-d", type=int, dest="device", default=0, help="Define device to use for calculations, default is 0")
args, unknownArgs = parser.parse_known_args()

if args.app == "." or args.tmp == ".":
	parser.print_help()

def quoted(text): return "\"{}\"".format(text)

tmpDirectory = ""
if os.path.exists(args.tmp):
	tmpDirectory = os.path.abspath(os.path.realpath(args.tmp))
	print("Found tmp directory:", tmpDirectory)
else:
	print('Unable to find tmp direcory \"{}\"'.format(args.tmp))
	exit(1)


def is_real_vtf_executable(path):
	#name = os.popen(path + " --version").read()
	name = "VTF"
	expected = "Vulkan Trivial Framework (VTF)"
	try:
		pipe = subprocess.run([path, '--version'], stdout = subprocess.PIPE)
		name = repr(pipe.stdout[0:len(expected)])
	except subprocess.CalledProcessError as e:
		print(e.output)
		pass
	return (expected in name)


def find_vtf_executable(path):
	if not os.path.exists(path): return None
	if os.path.isfile(path):
		if is_real_vtf_executable(path):
			return os.path.abspath(os.path.realpath(path))
	else:
		root,dirs,files = next(os.walk(path))
		for name in files:
			otherPath = os.path.join(root, name)
			if is_real_vtf_executable(otherPath):
				return os.path.abspath(os.path.realpath(otherPath))
	return None


vtfExecutable = find_vtf_executable(args.app)
if vtfExecutable:
	print("Found VTF executable:", vtfExecutable)
else:
	print("Unable to find VTF executable:", quoted(args.app))
	exit(1)


targetDirectory = ""
if os.path.exists(args.tgt):
	targetDirectory = os.path.abspath(os.path.realpath(args.tgt))
	print("Found target directory:", quoted(targetDirectory))
else:
	print("Unable to find target directory", quoted(args.tgt))
	exit(1)

shaderExtensions = [ ".vert", ".tesc", ".tese", ".geom", ".frag"]


def test_case_name_matches(testCaseName):
	if len(args.mask) == 0:
		return True
	else:
		result = True
		try:
			result = re.match(args.mask, testCaseName) != None
		except:
			result = False
		return result


def build_test_cases(path):
	if not os.path.exists(path): return {}
	cases = collections.defaultdict(dict)
	for root,dirs,files in os.walk(path):
		perFileName = collections.defaultdict(list)
		for fileName in files:
			groupFileName, shaderFileExt = os.path.splitext(fileName)
			if test_case_name_matches(groupFileName) and shaderFileExt in shaderExtensions:
				perFileName[groupFileName].append(shaderFileExt)
		if len(perFileName) > 0:
			cases[os.path.realpath(root)] = perFileName
	return cases


testCases = build_test_cases(targetDirectory)
if len(testCases) == 0:
	print("No shaders found to perform the tests")
	exit(0)


def perform_test_case(counter, vtf, tmp, root, shaderFileName, shaderFileExts):
	if ".vert" not in shaderFileExts or ".frag" not in shaderFileExts:
		print("Cannot execute \"{}\", missing vertex and fragment shaders", shaderFileName)
	else:
		command = [vtf, "-d", args.device, "-api", 11, "-spirv", 13, "-tmp", tmp]
		command.extend(["-l-no-vuid-undefined", "-l", "VK_LAYER_KHRONOS_validation"])
		command.extend(["int_graphics"])
		command.extend(unknownArgs)
		for shaderFileExt in shaderExtensions:
			if shaderFileExt in shaderFileExts:
				shaderFilePath = os.path.abspath(os.path.join(root, (shaderFileName + shaderFileExt)))
				command.extend([shaderFileExt.replace('.', '-'), shaderFilePath])
		output = ""
		try:
			if args.verbose:
				print(*map(str,command))
			else: print("Processing group", counter, ':', quoted(shaderFileName), shaderFileExts)
			output = subprocess.run(map(str,command), capture_output=True, shell=True, text=True)
			print(output.stdout)
		except:
			print(output.stderr)


def perform_test_cases(cases):
	for root,groupFileNames in zip(cases.keys(), cases.values()):
		counter = iter(range(len(groupFileNames)))
		for groupFileName,shaderFileExts in zip(groupFileNames.keys(), groupFileNames.values()):
			perform_test_case(next(counter), vtfExecutable, tmpDirectory, root, groupFileName, shaderFileExts)

perform_test_cases(testCases)

print("Script", quoted(os.path.basename(sys.argv[0])), "finished")
