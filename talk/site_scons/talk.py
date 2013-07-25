# Copyright 2010 Google Inc.
# All Rights Reserved.
#
# Author: Tim Haloun (thaloun@google.com)
#         Daniel Petersson (dape@google.com)
#
import os
import SCons.Util

class LibraryInfo:
  """Records information on the libraries defined in a build configuration.

  Attributes:
    lib_targets: Dictionary of library target params for lookups in
        ExtendComponent().
    prebuilt_libraries: Set of all prebuilt static libraries.
    system_libraries: Set of libraries not found in the above (used to detect
        out-of-order build rules).
  """

  # Dictionary of LibraryInfo objects keyed by BUILD_TYPE value.
  __library_info = {}

  @staticmethod
  def get(env):
    """Gets the LibraryInfo object for the current build type.

    Args:
      env: The environment object.

    Returns:
      The LibraryInfo object.
    """
    return LibraryInfo.__library_info.setdefault(env['BUILD_TYPE'],
                                                 LibraryInfo())

  def __init__(self):
    self.lib_targets = {}
    self.prebuilt_libraries = set()
    self.system_libraries = set()


def _GetLibParams(env, lib):
  """Gets the params for the given library if it is a library target.

  Returns the params that were specified when the given lib target name was
  created, or None if no such lib target has been defined. In the None case, it
  additionally records the negative result so as to detect out-of-order
  dependencies for future targets.

  Args:
    env: The environment object.
    lib: The library's name as a string.

  Returns:
    Its dictionary of params, or None.
  """
  info = LibraryInfo.get(env)
  if lib in info.lib_targets:
    return info.lib_targets[lib]
  else:
    if lib not in info.prebuilt_libraries and lib not in info.system_libraries:
      info.system_libraries.add(lib)
    return None


def _RecordLibParams(env, lib, params):
  """Record the params used for a library target.

  Record the params used for a library target while checking for several error
  conditions.

  Args:
    env: The environment object.
    lib: The library target's name as a string.
    params: Its dictionary of params.

  Raises:
    Exception: The lib target has already been recorded, or the lib was
        previously declared to be prebuilt, or the lib target is being defined
        after a reverse library dependency.
  """
  info = LibraryInfo.get(env)
  if lib in info.lib_targets:
    raise Exception('Multiple definitions of ' + lib)
  if lib in info.prebuilt_libraries:
    raise Exception(lib + ' already declared as a prebuilt library')
  if lib in info.system_libraries:
    raise Exception(lib + ' cannot be defined after its reverse library '
                    'dependencies')
  info.lib_targets[lib] = params


def _IsPrebuiltLibrary(env, lib):
  """Checks whether or not the given library is a prebuilt static library.

  Returns whether or not the given library name has been declared to be a
  prebuilt static library. In the False case, it additionally records the
  negative result so as to detect out-of-order dependencies for future targets.

  Args:
    env: The environment object.
    lib: The library's name as a string.

  Returns:
    True or False
  """
  info = LibraryInfo.get(env)
  if lib in info.prebuilt_libraries:
    return True
  else:
    if lib not in info.lib_targets and lib not in info.system_libraries:
      info.system_libraries.add(lib)
    return False


def _RecordPrebuiltLibrary(env, lib):
  """Record that a library is a prebuilt static library.

  Record that the given library name refers to a prebuilt static library while
  checking for several error conditions.

  Args:
    env: The environment object.
    lib: The library's name as a string.

  Raises:
    Exception: The lib has already been recorded to be prebuilt, or the lib was
        previously declared as a target, or the lib is being declared as
        prebuilt after a reverse library dependency.
  """
  info = LibraryInfo.get(env)
  if lib in info.prebuilt_libraries:
    raise Exception('Multiple prebuilt declarations of ' + lib)
  if lib in info.lib_targets:
    raise Exception(lib + ' already defined as a target')
  if lib in info.system_libraries:
    raise Exception(lib + ' cannot be declared as prebuilt after its reverse '
                    'library dependencies')
  info.prebuilt_libraries.add(lib)


def _GenericLibrary(env, static, **kwargs):
  """Extends ComponentLibrary to support multiplatform builds
     of dynamic or static libraries.

  Args:
    env: The environment object.
    kwargs: The keyword arguments.

  Returns:
    See swtoolkit ComponentLibrary
  """
  params = CombineDicts(kwargs, {'COMPONENT_STATIC': static})
  return ExtendComponent(env, 'ComponentLibrary', **params)


def DeclarePrebuiltLibraries(env, libraries):
  """Informs the build engine about external static libraries.

  Informs the build engine that the given external library name(s) are prebuilt
  static libraries, as opposed to shared libraries.

  Args:
    env: The environment object.
    libraries: The library or libraries that are being declared as prebuilt
        static libraries.
  """
  if not SCons.Util.is_List(libraries):
    libraries = [libraries]
  for library in libraries:
    _RecordPrebuiltLibrary(env, library)


def Library(env, **kwargs):
  """Extends ComponentLibrary to support multiplatform builds of static
     libraries.

  Args:
    env: The current environment.
    kwargs: The keyword arguments.

  Returns:
    See swtoolkit ComponentLibrary
  """
  return _GenericLibrary(env, True, **kwargs)


def DynamicLibrary(env, **kwargs):
  """Extends ComponentLibrary to support multiplatform builds
     of dynmic libraries.

  Args:
    env: The environment object.
    kwargs: The keyword arguments.

  Returns:
    See swtoolkit ComponentLibrary
  """
  return _GenericLibrary(env, False, **kwargs)


def Object(env, **kwargs):
  return ExtendComponent(env, 'ComponentObject', **kwargs)


def Unittest(env, **kwargs):
  """Extends ComponentTestProgram to support unittest built
     for multiple platforms.

  Args:
    env: The current environment.
    kwargs: The keyword arguments.

  Returns:
    See swtoolkit ComponentProgram.
  """
  kwargs['name'] = kwargs['name'] + '_unittest'

  common_test_params = {
    'posix_cppdefines': ['GUNIT_NO_GOOGLE3', 'GTEST_HAS_RTTI=0'],
    'libs': ['unittest_main', 'gunit']
  }
  if 'explicit_libs' not in kwargs:
    common_test_params['win_libs'] = [
      'advapi32',
      'crypt32',
      'iphlpapi',
      'secur32',
      'shell32',
      'shlwapi',
      'user32',
      'wininet',
      'ws2_32'
    ]
    common_test_params['lin_libs'] = [
      'crypto',
      'pthread',
      'ssl',
    ]

  params = CombineDicts(kwargs, common_test_params)
  return ExtendComponent(env, 'ComponentTestProgram', **params)


def App(env, **kwargs):
  """Extends ComponentProgram to support executables with platform specific
     options.

  Args:
    env: The current environment.
    kwargs: The keyword arguments.

  Returns:
    See swtoolkit ComponentProgram.
  """
  if 'explicit_libs' not in kwargs:
    common_app_params = {
      'win_libs': [
        'advapi32',
        'crypt32',
        'iphlpapi',
        'secur32',
        'shell32',
        'shlwapi',
        'user32',
        'wininet',
        'ws2_32'
      ]}
    params = CombineDicts(kwargs, common_app_params)
  else:
    params = kwargs
  return ExtendComponent(env, 'ComponentProgram', **params)

def WiX(env, **kwargs):
  """ Extends the WiX builder
  Args:
    env: The current environment.
    kwargs: The keyword arguments.

  Returns:
    The node produced by the environment's wix builder
  """
  return ExtendComponent(env, 'WiX', **kwargs)

def Repository(env, at, path):
  """Maps a directory external to $MAIN_DIR to the given path so that sources
     compiled from it end up in the correct place under $OBJ_DIR.  NOT required
     when only referring to header files.

  Args:
    env: The current environment object.
    at: The 'mount point' within the current directory.
    path: Path to the actual directory.
  """
  env.Dir(at).addRepository(env.Dir(path))


def Components(*paths):
  """Completes the directory paths with the correct file
     names such that the directory/directory.scons name
     convention can be used.

  Args:
    paths: The paths to complete. If it refers to an existing
           file then it is ignored.

  Returns:
    The completed lif scons files that are needed to build talk.
  """
  files = []
  for path in paths:
    if os.path.isfile(path):
      files.append(path)
    else:
      files.append(ExpandSconsPath(path))
  return files


def ExpandSconsPath(path):
  """Expands a directory path into the path to the
     scons file that our build uses.
     Ex: magiflute/plugin/common => magicflute/plugin/common/common.scons

  Args:
    path: The directory path to expand.

  Returns:
    The expanded path.
  """
  return '%s/%s.scons' % (path, os.path.basename(path))


def ReadVersion(filename):
  """Executes the supplied file and pulls out a version definition from it. """
  defs = {}
  execfile(str(filename), defs)
  if 'version' not in defs:
    return '0.0.0.0'
  version = defs['version']
  parts = version.split(',')
  build = os.environ.get('GOOGLE_VERSION_BUILDNUMBER')
  if build:
    parts[-1] = str(build)
  return '.'.join(parts)


#-------------------------------------------------------------------------------
# Helper methods for translating talk.Foo() declarations in to manipulations of
# environmuent construction variables, including parameter parsing and merging,
#
def PopEntry(dictionary, key):
  """Get the value from a dictionary by key. If the key
     isn't in the dictionary then None is returned. If it is in
     the dictionary the value is fetched and then is it removed
     from the dictionary.

  Args:
    dictionary: The dictionary.
    key: The key to get the value for.
  Returns:
    The value or None if the key is missing.
  """
  value = None
  if key in dictionary:
    value = dictionary[key]
    dictionary.pop(key)
  return value


def MergeAndFilterByPlatform(env, params):
  """Take a dictionary of arguments to lists of values, and, depending on
     which platform we are targetting, merge the lists of associated keys.
     Merge by combining value lists like so:
       {win_foo = [a,b], lin_foo = [c,d], foo = [e], mac_bar = [f], bar = [g] }
       becomes {foo = [a,b,e], bar = [g]} on windows, and
       {foo = [e], bar = [f,g]} on mac

  Args:
    env: The hammer environment which knows which platforms are active
    params: The keyword argument dictionary.
  Returns:
    A new dictionary with the filtered and combined entries of params
  """
  platforms = {
    'linux': 'lin_',
    'mac': 'mac_',
    'posix': 'posix_',
    'windows': 'win_',
  }
  active_prefixes = [
    platforms[x] for x in iter(platforms) if env.Bit(x)
  ]
  inactive_prefixes = [
    platforms[x] for x in iter(platforms) if not env.Bit(x)
  ]

  merged = {}
  for arg, values in params.iteritems():
    inactive_platform = False

    key = arg

    for prefix in active_prefixes:
      if arg.startswith(prefix):
        key = arg[len(prefix):]

    for prefix in inactive_prefixes:
      if arg.startswith(prefix):
        inactive_platform = True

    if inactive_platform:
      continue

    AddToDict(merged, key, values)

  return merged


def MergeSettingsFromLibraryDependencies(env, params):
  if 'libs' in params:
    for lib in params['libs']:
      libparams = _GetLibParams(env, lib)
      if libparams:
        if 'dependent_target_settings' in libparams:
          params = CombineDicts(
              params,
              MergeAndFilterByPlatform(
                  env,
                  libparams['dependent_target_settings']))
  return params


def ExtendComponent(env, component, **kwargs):
  """A wrapper around a scons builder function that preprocesses and post-
     processes its inputs and outputs.  For example, it merges and filters
     certain keyword arguments before appending them to the environments
     construction variables.  It can build signed targets and 64bit copies
     of targets as well.

  Args:
    env: The hammer environment with which to build the target
    component: The environment's builder function, e.g. ComponentProgram
    kwargs: keyword arguments that are either merged, translated, and passed on
            to the call to component, or which control execution.
            TODO(): Document the fields, such as cppdefines->CPPDEFINES,
            prepend_includedirs, include_talk_media_libs, etc.
  Returns:
    The output node returned by the call to component, or a subsequent signed
    dependant node.
  """
  env = env.Clone()

  # prune parameters intended for other platforms, then merge
  params = MergeAndFilterByPlatform(env, kwargs)

  # get the 'target' field
  name = PopEntry(params, 'name')

  # get the 'packages' field and process it if present (only used for Linux).
  packages = PopEntry(params, 'packages')
  if packages and len(packages):
    params = CombineDicts(params, env.GetPackageParams(packages))

  # save pristine params of lib targets for future reference
  if 'ComponentLibrary' == component:
    _RecordLibParams(env, name, dict(params))

  # add any dependent target settings from library dependencies
  params = MergeSettingsFromLibraryDependencies(env, params)

  # if this is a signed binary we need to make an unsigned version first
  signed = env.Bit('windows') and PopEntry(params, 'signed')
  if signed:
    name = 'unsigned_' + name

  # potentially exit now
  srcs = PopEntry(params, 'srcs')
  if not srcs or not hasattr(env, component):
    return None

  # apply any explicit dependencies
  dependencies = PopEntry(params, 'depends')
  if dependencies is not None:
    env.Depends(name, dependencies)

  # put the contents of params into the environment
  # some entries are renamed then appended, others renamed then prepended
  appends = {
    'cppdefines' : 'CPPDEFINES',
    'libdirs' : 'LIBPATH',
    'link_flags' : 'LINKFLAGS',
    'libs' : 'LIBS',
    'FRAMEWORKS' : 'FRAMEWORKS',
  }
  prepends = {}
  if env.Bit('windows'):
    # MSVC compile flags have precedence at the beginning ...
    prepends['ccflags'] = 'CCFLAGS'
  else:
    # ... while GCC compile flags have precedence at the end
    appends['ccflags'] = 'CCFLAGS'
  if PopEntry(params, 'prepend_includedirs'):
    prepends['includedirs'] = 'CPPPATH'
  else:
    appends['includedirs'] = 'CPPPATH'

  for field, var in appends.items():
    values = PopEntry(params, field)
    if values is not None:
      env.Append(**{var : values})
  for field, var in prepends.items():
    values = PopEntry(params, field)
    if values is not None:
      env.Prepend(**{var : values})

  # any other parameters are replaced without renaming
  for field, value in params.items():
    env.Replace(**{field : value})

  if env.Bit('linux') and 'LIBS' in env:
    libs = env['LIBS']
    # When using --as-needed + --start/end-group, shared libraries need to come
    # after --end-group on the command-line because the pruning decision only
    # considers the preceding modules and --start/end-group may cause the
    # effective position of early static libraries on the command-line to be
    # deferred to the point of --end-group. To effect this, we move shared libs
    # into _LIBFLAGS, which has the --end-group as its first entry. SCons does
    # not track dependencies on system shared libraries anyway so we lose
    # nothing by removing them from LIBS.
    static_libs = [lib for lib in libs if
                   _GetLibParams(env, lib) or _IsPrebuiltLibrary(env, lib)]
    shared_libs = ['-l' + lib for lib in libs if not
                   (_GetLibParams(env, lib) or _IsPrebuiltLibrary(env, lib))]
    env.Replace(LIBS=static_libs)
    env.Append(_LIBFLAGS=shared_libs)

  # invoke the builder function
  builder = getattr(env, component)

  node = builder(name, srcs)

  if env.Bit('mac') and 'ComponentProgram' == component:
    # Build .dSYM debug packages. This is useful even for non-stripped
    # binaries, as the dsym utility will fetch symbols from all
    # statically-linked libraries (the linker doesn't include them in to the
    # final binary).
    build_dsym = env.Command(
        env.Dir('$STAGING_DIR/%s.dSYM' % node[0]),
        node,
        'mkdir -p `dirname $TARGET` && dsymutil -o $TARGET $SOURCE')
    env.Alias('all_dsym', env.Alias('%s.dSYM' % node[0], build_dsym))

  if signed:
    # Get the name of the built binary, then get the name of the final signed
    # version from it.  We need the output path since we don't know the file
    # extension beforehand.
    target = node[0].path.split('_', 1)[1]
    # postsignprefix: If defined, postsignprefix is a string that should be
    # prepended to the target executable.  This is to provide a work around
    # for EXEs and DLLs with the same name, which thus have PDBs with the
    # same name.  Setting postsignprefix allows the EXE and its PDB
    # to be renamed and copied in a previous step; then the desired
    # name of the EXE (but not PDB) is reconstructed after signing.
    postsignprefix = PopEntry(params, 'postsignprefix')
    if postsignprefix is not None:
      target = postsignprefix + target
    signed_node = env.SignedBinary(
      source = node,
      target = '$STAGING_DIR/' + target,
    )
    env.Alias('signed_binaries', signed_node)
    return signed_node

  return node


def AddToDict(dictionary, key, values, append=True):
  """Merge the given key value(s) pair into a dictionary.  If it contains an
     entry with that key already, then combine by appending or prepending the
     values as directed.  Otherwise, assign a new keyvalue pair.
  """
  if values is None:
    return

  if key not in dictionary:
    dictionary[key] = values
    return

  cur = dictionary[key]
  # TODO(dape): Make sure that there are no duplicates
  # in the list. I can't use python set for this since
  # the nodes that are returned by the SCONS builders
  # are not hashable.
  # dictionary[key] = list(set(cur).union(set(values)))
  if append:
    dictionary[key] = cur + values
  else:
    dictionary[key] = values + cur


def CombineDicts(a, b):
  """Unions two dictionaries of arrays/dictionaries.

  Unions two dictionaries of arrays/dictionaries by combining the values of keys
  shared between them. The original dictionaries should not be used again after
  this call.

  Args:
    a: First dict.
    b: Second dict.

  Returns:
    The union of a and b.
  """
  c = {}
  for key in a:
    if key in b:
      aval = a[key]
      bval = b.pop(key)
      if isinstance(aval, dict) and isinstance(bval, dict):
        c[key] = CombineDicts(aval, bval)
      else:
        c[key] = aval + bval
    else:
      c[key] = a[key]

  for key in b:
    c[key] = b[key]

  return c


def RenameKey(d, old, new, append=True):
  AddToDict(d, new, PopEntry(d, old), append)
