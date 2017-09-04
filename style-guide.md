# WebRTC coding style guide

## C++

WebRTC follows the [Chromium][chr-style] and [Google][goog-style] C++
style guides, unless an exception is listed below. In cases where they
conflict, the Chromium style guide trumps the Google style guide, and
the exceptions in this file trump them both.

[chr-style]: https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md
[goog-style]: https://google.github.io/styleguide/cppguide.html

Some older parts of the code violate the style guide in various ways.

* If making small changes to such code, follow the style guide when
  it’s reasonable to do so, but in matters of formatting etc., it is
  often better to be consistent with the surrounding code.
* If making large changes to such code, consider first cleaning it up
  in a separate CL.

### Exceptions

There are no exceptions yet. If and when exceptions are adopted,
they’ll be listed here.

## C

There’s a substantial chunk of legacy C code in WebRTC, and a lot of
it is old enough that it violates the parts of the C++ style guide
that also applies to C (naming etc.) for the simple reason that it
pre-dates the use of the current C++ style guide for this code base.

* If making small changes to C code, mimic the style of the
  surrounding code.
* If making large changes to C code, consider converting the whole
  thing to C++ first.
