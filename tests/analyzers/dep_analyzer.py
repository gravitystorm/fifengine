#!/usr/bin/python
import sys, re, os

if '.' not in sys.path:
	sys.path.append('.')
from utils.util_scripts.path import path

# the following dictionary defines the allowed dependencies checked while testing
# format = user -> providers
ALLOWED_MODULE_DEPS = {
	'controller': ('model', 'eventchannel', 'gui', 'video', 'loaders', 'view', 'audio', 'util', 'vfs'),
	'model': ('util',),
	'eventchannel': ('util',),
	'gui': ('eventchannel', 'util', 'video', 'vfs'),
	'video': ('util',),
	'loaders': ('video', 'vfs', 'xml', 'util', 'view', 'model'),
	'view': ('model', 'video', 'util', 'audio'),
	'audio': ('util', 'vfs'),
	'util': [],
	'vfs': ('util',),
	'xml': ('vfs', 'util'),
	'pathfinder': ('util', 'model'),
}

_S = os.path.sep
ROOTDIRNAME = 'engine/core'
FILE_DEPS_OUT = 'doc/dependencies/filedeps'
DIR_DEPS_OUT = 'doc/dependencies/dirdeps'
MODULE_DEPS_OUT = 'doc/dependencies/moduledeps'
SKIPPED_PROVIDERS = []
SKIPPED_USERS = []

reInc = re.compile('#include "(.*?)"')

def add_to_dictset(d, key, val):
	try:
		d[key].add(val)
	except KeyError:
		d[key] = set([val])
	
# return error string in case of possible error
unknown_modules = []
def check_dep_error(allowed_deps, user, provider):
	global unknown_modules
	msg = ''
	try:
		if not provider in allowed_deps[user]:
			msg = 'Illegal dependency between %s -> %s, % s can use only:\n' % (user, provider, user)
			for d in allowed_deps[user]:
				msg += '   %s\n' % d
	except KeyError:
		print unknown_modules
		if user not in unknown_modules:
			msg = 'Unknown module %s found in static check\n' % user
			msg += '   please adjust dep_analyzer script to match new structure'
			unknown_modules.append(user)
	return msg

def get_file2inc(sources):
	file2inc = {}
	for f in sources:
		inComment = False
		for line in open(f):
			if not inComment and line.find('/*') != -1:
				inComment = True
				continue
			elif inComment:
				if line.find('*/') != -1:
					inComment = False
				continue
			elif line.strip().startswith('//'):
				continue
			m = reInc.search(line)
			if m:
				add_to_dictset(file2inc, f, m.group(1))
	return file2inc

def fill_dep_infos(file2inc, fileUser2provider, dirUser2provider, moduleUser2provider, unknownIncludes, dirclusters):
	for f, incs in file2inc.items():
		#if f.find('engine.cpp') != -1:
		#	import pdb; pdb.set_trace()
		skip = False
		for user in SKIPPED_USERS:
			if f.find(user) != -1:
				skip = True
				break
		if skip:
			continue
			
		for i in incs:
			user = str(f.dirname()).replace(ROOTDIRNAME + _S, '')
			header = path((f.dirname() / path(i)).abspath().split(ROOTDIRNAME + _S)[1])
			if not header.isfile():
				header = path(ROOTDIRNAME) / path(i)
				if not header.isfile():
					add_to_dictset(unknownIncludes, str(f), str(i))
					continue
			provider = str(header.dirname()).replace(ROOTDIRNAME + _S, '')
			skip = False
			for skipped in SKIPPED_PROVIDERS:
				if header.find(skipped) != -1:
					skip = True
					break
			if skip:
				continue
			add_to_dictset(dirUser2provider, user, provider)
			usermodule = user.split(_S)[0]
			providermodule = provider.split(_S)[0]
			userfile = user.split(_S)[-1].split('.')[0]
			providerfile = provider.split(_S)[-1].split('.')[0]

			add_to_dictset(dirclusters, usermodule, user)
			add_to_dictset(dirclusters, providermodule, provider)
			add_to_dictset(moduleUser2provider, usermodule, providermodule)
			add_to_dictset(fileUser2provider, userfile, providerfile)

def write_dot_file(fname, contents):
	lines = []
	a = lines.append
	a('digraph "source tree" {')
	a('    overlap=scale;')
	a('    size="8,10";')
	a('    ratio="fill";')
	a('    fontsize="16";')
	a('    fontname="Helvetica";')
	a('    clusterrank="local";')
	if type(contents) in (list, tuple):
		lines += contents
	else:
		lines.append(contents)
	a('}')
	open(fname, 'w').write('\n'.join(lines))

def get_cluster_str(ind, elements, label):
	lines = []
	a = lines.append
	a('subgraph cluster_%d {' % ind)
	a('    style=filled;')
	a('    color=lightgrey;')
	a('    node [style=filled,color=white];')
	a('    %s' % '; '.join('"%s"' % d for d in elements))
	a('    label = "%s";' % label)
	a('}')
	return '\n'.join(lines)

def analyze(write_postscript=False):
	root = path(ROOTDIRNAME)
	headers = list(root.walkfiles('*.h'))
	sources = headers + list(root.walkfiles('*.cpp'))
	file2inc = get_file2inc(sources)
	
	moduleUser2provider = {}
	dirUser2provider = {}
	fileUser2provider = {}	
	unknownIncludes = {}
	dirclusters = {}
	fill_dep_infos(file2inc, fileUser2provider, dirUser2provider, moduleUser2provider, unknownIncludes, dirclusters)
	
	# write module dep graph
	out = []
	illegalModuleDeps = []
	for user, providers in sorted(moduleUser2provider.items()):
		for provider in sorted(providers):
			if user != provider:
				out.append('    "' + user + '" -> "' + provider + '"')
				msg = check_dep_error(ALLOWED_MODULE_DEPS, user, provider)
				if msg:
					illegalModuleDeps.append(msg)
	write_dot_file('%s.dot' % MODULE_DEPS_OUT, out)
	if write_postscript:
		os.system('dot -Tps %s.dot > %s.ps' % (MODULE_DEPS_OUT, MODULE_DEPS_OUT))
	os.system('dot -Tpng %s.dot	 > %s.png' % (MODULE_DEPS_OUT, MODULE_DEPS_OUT))
	
	# write dir dep graph
	out = []
	for cluster, subdirs in sorted(dirclusters.items()):
		out.append(get_cluster_str(len(out), sorted(subdirs), cluster))
	for user, providers in sorted(dirUser2provider.items()):
		for provider in sorted(providers):
			if user != provider:
				out.append('    "' + user + '" -> "' + provider + '"')
	write_dot_file('%s.dot' % DIR_DEPS_OUT, out)
	if write_postscript:
		os.system('dot -Tps %s.dot > %s.ps' % (DIR_DEPS_OUT, DIR_DEPS_OUT))
	
	# write file dep graph
	out = []
	for user, providers in sorted(file2inc.items()):
		for provider in sorted(providers):
			if user != provider:
				out.append('    "' + user + '" -> "' + provider + '"')
	write_dot_file('%s.dot' % FILE_DEPS_OUT, out)
	# os.system('dot -Tps %s.dot > %s.ps' % (MODULE_DEPS_OUT, MODULE_DEPS_OUT))
	
	# write raw dep info
	#out = []
	#for f, file2inc
	return '\n'.join(illegalModuleDeps)

_ANALYZE_FN_ = analyze

if __name__ == '__main__':
	print analyze(True)
