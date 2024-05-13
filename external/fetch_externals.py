import os
import sys
import stat
import subprocess


def verify_git():
	fnd = False
	exp = "git version"
	try:
		out = subprocess.run(["git", "--version"], stdout=subprocess.PIPE)
		ver = out.stdout[0:len(exp)].decode(sys.stdout.encoding)
		fnd = ver == exp
	except subprocess.CalledProcessError as e:
		pass
	return fnd

if not verify_git():
	print("GIT not found, please install it")
	exit(1)

PWD = os.path.dirname(__file__)
#print(PWD)

def enable_rw(path):
	if os.path.exists(path):
		mode = os.stat(path).st_mode | stat.S_IWUSR
		os.chmod(path, mode)
		return True
	return False

def remove_directory_content(directory):
	content = []
	def dive(path):
		for root,dirs,files in os.walk(path):
			for filename in files:
				content.append( (os.path.join(root, filename), False))
			for dirname in dirs:
				where = os.path.join(root, dirname)
				content.append( (where, True) )
				dive(where)
	dive(directory)
	content = sorted(content, key=lambda info: len(info[0]), reverse=True)
	for item in content:
		#print(item[0], item[1])
		path = os.path.abspath(item[0])
		if enable_rw(path):
			if item[1]:
				os.rmdir(path)
			else: os.remove(path)


available_repos = [
	( "stb",		"https://github.com/nothings/stb.git",			"master",   False),
	( "stl_reader",	"https://github.com/sreiter/stl_reader.git",	"master",   False),
]

required_repos = []
if len(sys.argv) == 1:
	#print("Force download all repos:", len(sys.argv))
	required_repos = available_repos
else:
	def make_repo_required(repo):
		return (repo[0], repo[1], repo[2], True)
	for repo_name in sys.argv[1:]:
		for repo in available_repos:
			if repo_name == repo[0]:
				#print("Assign repo to download:", repo_name)
				required_repos.append(make_repo_required(repo))
				break

#for repo in required_repos: print("Repo: ", repo)

def pull_repo(repo):
	init = ["git",  "init", "-b", "main"]
	orig = ["git", "remote", "add", "origin", repo[1]]
	pull = ["git", "pull", "origin", repo[2]]
	try:
		subprocess.run(init)
		subprocess.run(orig)
		subprocess.run(pull)
	except subprocess.CalledProcessError as e:
		print(e.output)

for repo in required_repos:
	check = repo[3]
	path = os.path.join(PWD, repo[0])
	if not os.path.exists(path): os.mkdir(path)
	if not check or not os.path.exists(os.path.join(path, ".git")):
		print("Enter directory", path)
		print("Removing old repository")
		remove_directory_content(path)
		os.chdir(path)
		pull_repo(repo)
		os.chdir(PWD)



