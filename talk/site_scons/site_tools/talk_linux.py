# Copyright 2010 Google Inc.
# All Rights Reserved.
# Author: tschmelcher@google.com (Tristan Schmelcher)

"""Tool for helpers used in linux building process."""

import os
import SCons.Defaults
import subprocess


def _OutputFromShellCommand(command):
  process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
  return process.communicate()[0].strip()


# This is a pure SCons helper function.
def _InternalBuildDebianPackage(env, debian_files, package_files,
    output_dir=None, force_version=None):
  """Creates build rules to build a Debian package from the specified sources.

  Args:
    env: SCons Environment.
    debian_files: Array of the Debian control file sources that should be
        copied into the package source tree, e.g., changelog, control, rules,
        etc.
    package_files: An array of 2-tuples listing the files that should be
        copied into the package source tree.
        The first element is the path where the file should be placed for the
            .install control file to find it, relative to the generated debian
            package source directory.
        The second element is the file source.
    output_dir: An optional directory to place the files in. If omitted, the
        current output directory is used.
    force_version: Optional. Forces the version of the package to start with
        this version string if specified. If the last entry in the changelog
        is not for a version that starts with this then a dummy entry is
        generated with this version and a ~prerelease suffix (so that the
        final version will compare as greater).

  Return:
    A list of the targets (if any).
  """
  if 0 != subprocess.call(['which', 'dpkg-buildpackage']):
    print ('dpkg-buildpackage not installed on this system; '
           'skipping DEB build stage')
    return []
  # Read the control file and changelog file to determine the package name,
  # version, and arch that the Debian build tools will use to name the
  # generated files.
  control_file = None
  changelog_file = None
  for file_name in debian_files:
    if os.path.basename(file_name) == 'control':
      control_file = env.File(file_name).srcnode().abspath
    elif os.path.basename(file_name) == 'changelog':
      changelog_file = env.File(file_name).srcnode().abspath
  if not control_file:
    raise Exception('Need to have a control file')
  if not changelog_file:
    raise Exception('Need to have a changelog file')
  source = _OutputFromShellCommand(
      "awk '/^Source:/ { print $2; }' " + control_file)
  packages = _OutputFromShellCommand(
      "awk '/^Package:/ { print $2; }' " + control_file).split('\n')
  version = _OutputFromShellCommand(
      "sed -nr '1 { s/.*\\((.*)\\).*/\\1/; p }' " + changelog_file)
  arch = _OutputFromShellCommand('dpkg --print-architecture')
  add_dummy_changelog_entry = False
  if force_version and not version.startswith(force_version):
    print ('Warning: no entry in ' + changelog_file + ' for version ' +
        force_version + ' (last is ' + version +'). A dummy entry will be ' +
        'generated. Remember to add the real changelog entry before ' +
        'releasing.')
    version = force_version + '~prerelease'
    add_dummy_changelog_entry = True
  source_dir_name = source + '_' + version + '_' + arch
  target_file_names = [ source_dir_name + '.changes' ]
  for package in packages:
    package_file_name = package + '_' + version + '_' + arch + '.deb'
    target_file_names.append(package_file_name)
  # The targets
  if output_dir:
    targets = [os.path.join(output_dir, s) for s in target_file_names]
  else:
    targets = target_file_names
  # Path to where we will construct the debian build tree.
  deb_build_tree = os.path.join(source_dir_name, 'deb_build_tree')
  # First copy the files.
  for file_name in package_files:
    env.Command(os.path.join(deb_build_tree, file_name[0]), file_name[1],
        SCons.Defaults.Copy('$TARGET', '$SOURCE'))
    env.Depends(targets, os.path.join(deb_build_tree, file_name[0]))
  # Now copy the Debian metadata sources. We have to do this all at once so
  # that we can remove the target directory before copying, because there
  # can't be any other stale files there or else dpkg-buildpackage may use
  # them and give incorrect build output.
  copied_debian_files_paths = []
  for file_name in debian_files:
    copied_debian_files_paths.append(os.path.join(deb_build_tree, 'debian',
                                                  os.path.basename(file_name)))
  copy_commands = [
      """dir=$$(dirname $TARGET) && \
          rm -Rf $$dir && \
          mkdir -p $$dir && \
          cp $SOURCES $$dir && \
          chmod -R u+w $$dir"""
  ]
  if add_dummy_changelog_entry:
    copy_commands += [
        """debchange -c $$(dirname $TARGET)/changelog --newversion %s \
            --distribution UNRELEASED \
            'Developer preview build. (This entry was auto-generated.)'""" %
        version
    ]
  env.Command(copied_debian_files_paths, debian_files, copy_commands)
  env.Depends(targets, copied_debian_files_paths)
  # Must explicitly specify -a because otherwise cross-builds won't work.
  # Must explicitly specify -D because -a disables it.
  # Must explicitly specify fakeroot because old dpkg tools don't assume that.
  env.Command(targets, None,
      """dir=%(dir)s && \
          cd $$dir && \
          dpkg-buildpackage -b -uc -a%(arch)s -D -rfakeroot && \
          cd $$OLDPWD && \
          for file in %(targets)s; do \
            mv $$dir/../$$file $$(dirname $TARGET) || exit 1; \
          done""" %
      {'dir':env.Dir(deb_build_tree).path,
       'arch':arch,
       'targets':' '.join(target_file_names)})
  return targets


def BuildDebianPackage(env, debian_files, package_files, force_version=None):
  """Creates build rules to build a Debian package from the specified sources.

  This is a Hammer-ified version of _InternalBuildDebianPackage that knows to
  put the packages in the Hammer staging dir.

  Args:
    env: SCons Environment.
    debian_files: Array of the Debian control file sources that should be
        copied into the package source tree, e.g., changelog, control, rules,
        etc.
    package_files: An array of 2-tuples listing the files that should be
        copied into the package source tree.
        The first element is the path where the file should be placed for the
            .install control file to find it, relative to the generated debian
            package source directory.
        The second element is the file source.
    force_version: Optional. Forces the version of the package to start with
        this version string if specified. If the last entry in the changelog
        is not for a version that starts with this then a dummy entry is
        generated with this version and a ~prerelease suffix (so that the
        final version will compare as greater).

  Return:
    A list of the targets (if any).
  """
  if not env.Bit('host_linux'):
    return []
  return _InternalBuildDebianPackage(env, debian_files, package_files,
      output_dir='$STAGING_DIR', force_version=force_version)


def _GetPkgConfigCommand():
  """Return the pkg-config command line to use.

  Returns:
    A string specifying the pkg-config command line to use.
  """
  return os.environ.get('PKG_CONFIG') or 'pkg-config'


def _EscapePosixShellArgument(arg):
  """Escapes a shell command line argument so that it is interpreted literally.

  Args:
    arg: The shell argument to escape.

  Returns:
    The escaped string.
  """
  return "'%s'" % arg.replace("'", "'\\''")


def _HavePackage(package):
  """Whether the given pkg-config package name is present on the build system.

  Args:
    package: The name of the package.

  Returns:
    True if the package is present, else False
  """
  return subprocess.call('%s --exists %s' % (
      _GetPkgConfigCommand(),
      _EscapePosixShellArgument(package)), shell=True) == 0


def _GetPackageFlags(flag_type, packages):
  """Get the flags needed to compile/link against the given package(s).

  Returns the flags that are needed to compile/link against the given pkg-config
  package(s).

  Args:
    flag_type: The option to pkg-config specifying the type of flags to get.
    packages: The list of package names as strings.

  Returns:
    The flags of the requested type.

  Raises:
    subprocess.CalledProcessError: The pkg-config command failed.
  """
  pkg_config = _GetPkgConfigCommand()
  command = ' '.join([pkg_config] +
                     [_EscapePosixShellArgument(arg) for arg in
                      [flag_type] + packages])
  process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
  output = process.communicate()[0]
  if process.returncode != 0:
    raise subprocess.CalledProcessError(process.returncode, pkg_config)
  return output.strip().split(' ')


def GetPackageParams(env, packages):
  """Get the params needed to compile/link against the given package(s).

  Returns the params that are needed to compile/link against the given
  pkg-config package(s).

  Args:
    env: The current SCons environment.
    packages: The name of the package, or a list of names.

  Returns:
    A dictionary containing the params.

  Raises:
    Exception: One or more of the packages is not installed.
  """
  if not env.Bit('host_linux'):
    return {}
  if not SCons.Util.is_List(packages):
    packages = [packages]
  for package in packages:
    if not _HavePackage(package):
      raise Exception(('Required package \"%s\" was not found. Please install '
                       'the package that provides the \"%s.pc\" file.') %
                      (package, package))
  package_ccflags = _GetPackageFlags('--cflags', packages)
  package_libs = _GetPackageFlags('--libs', packages)
  # Split package_libs into libs, libdirs, and misc. linker flags. (In a perfect
  # world we could just leave libdirs in link_flags, but some linkers are
  # somehow confused by the different argument order.)
  libs = [flag[2:] for flag in package_libs if flag[0:2] == '-l']
  libdirs = [flag[2:] for flag in package_libs if flag[0:2] == '-L']
  link_flags = [flag for flag in package_libs if flag[0:2] not in ['-l', '-L']]
  return {
      'ccflags': package_ccflags,
      'libs': libs,
      'libdirs': libdirs,
      'link_flags': link_flags,
      'dependent_target_settings' : {
          'libs': libs[:],
          'libdirs': libdirs[:],
          'link_flags': link_flags[:],
      },
  }


def EnableFeatureWherePackagePresent(env, bit, cpp_flag, package):
  """Enable a feature if a required pkg-config package is present.

  Args:
    env: The current SCons environment.
    bit: The name of the Bit to enable when the package is present.
    cpp_flag: The CPP flag to enable when the package is present.
    package: The name of the package.
  """
  if not env.Bit('host_linux'):
    return
  if _HavePackage(package):
    env.SetBits(bit)
    env.Append(CPPDEFINES=[cpp_flag])
  else:
    print ('Warning: Package \"%s\" not found. Feature \"%s\" will not be '
           'built. To build with this feature, install the package that '
           'provides the \"%s.pc\" file.') % (package, bit, package)

def GetGccVersion(env):
  if env.Bit('cross_compile'):
    gcc_command = env['CXX']
  else:
    gcc_command = 'gcc'
  version_string = _OutputFromShellCommand(
      '%s --version | head -n 1 |'
      r'sed "s/.*\([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/g"' % gcc_command)
  return tuple([int(x or '0') for x in version_string.split('.')])

def generate(env):
  if env.Bit('linux'):
    env.AddMethod(EnableFeatureWherePackagePresent)
    env.AddMethod(GetPackageParams)
    env.AddMethod(BuildDebianPackage)
    env.AddMethod(GetGccVersion)


def exists(env):
  return 1  # Required by scons
