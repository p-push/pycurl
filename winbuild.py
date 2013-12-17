root = 'c:/dev/build-pycurl'
# where msysgit is installed
git_root = 'c:/program files/git'
libcurl_version = '7.34.0'

import os, os.path, sys, subprocess, shutil

archives_path = os.path.join(root, 'archives')
git_bin_path = os.path.join(git_root, 'bin')
git_path = os.path.join(git_bin_path, 'git')
tar_path = ['tar']

try:
    from urllib.request import urlopen
except ImportError:
    from urllib import urlopen

def fetch(url, archive=None):
    if archive is None:
        archive = os.path.basename(url)
    if not os.path.exists(archive):
        sys.stdout.write("Fetching %s\n" % url)
        io = urlopen(url)
        with open('.tmp.%s' % archive, 'wb') as f:
            while True:
                chunk = io.read(65536)
                if len(chunk) == 0:
                    break
                f.write(chunk)
        os.rename('.tmp.%s' % archive, archive)

def work():
	os.environ['PATH'] += ";%s" % git_bin_path
	if not os.path.exists(archives_path):
		os.makedirs(archives_path)
	os.chdir(archives_path)
	fetch('http://curl.haxx.se/download/curl-%s.tar.gz' % libcurl_version)
	if os.path.exists('curl-%s' % libcurl_version):
		shutil.rmtree('curl-%s' % libcurl_version)
	subprocess.check_call([tar_path, 'xf', 'curl-%s.tar.gz' % libcurl_version])
	os.chdir(os.path.join('curl-%s' % libcurl_version, 'winbuild'))
	subprocess.check_call(['nmake', '/f', 'Makefile.vc', 'mode=static', 'ENABLE_IDN=no'])
	subprocess.check_call(['nmake', '/f', 'Makefile.vc', 'mode=dll', 'ENABLE_IDN=no'])

work()
