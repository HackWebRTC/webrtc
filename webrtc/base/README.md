# What?

The contents of base have moved to rtc_base.

# Why?

We want to move all the contents in the webrtc directory to top-level, because:
* When we migrate from Rietveld to PolyGerrit we won't be able to apply WebRTC
patches on Chromium trybots (which is a very useful feature, especially as we
plan to have such trybots in our default trybot set).
This is because PolyGerrit needs the repo in Chromium to be the same as the
WebRTC repo, otherwise it won’t be able to apply the patch.
* To fully automate rolling into Chromium DEPS, we wish to use the LKGR finder,
but doing so is blocked on this (needs the same revision hashes to avoid ugly
hacks). See [this bug](http://crbug.com/666726).

# Tracking Bugs

[Chromium tracking bug](http://crbug.com/611808)
[WebRTC tracking bug](https://bugs.chromium.org/p/webrtc/issues/detail?id=7634)
