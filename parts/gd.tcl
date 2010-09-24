namespace eval cookit::gd {}

cookit::partRegister gd "GD library"

proc cookit::gd::parameters {version} {
    return [list \
        provides {} depends {} \
	buildmodes {dynamic} \
        ]
}

proc cookit::gd::initialize-static {version} {
}

proc cookit::gd::configure-static {} {
}

proc cookit::gd::build-static {} {
}

proc cookit::gd::install-static {} {
}

proc cookit::gd::vfsfilelist-static {} {
    set filelist [list]
    return $filelist
}

proc cookit::gd::linkerflags-static {} {
    return [list]
}

proc cookit::gd::linkerfiles-static {} {
    set rc [list]
    return $rc
}

proc cookit::gd::initialize-dynamic {version} {
}

proc cookit::gd::configure-dynamic {} {
    cookit::buildConfigure -sourcepath relative \
        -mode dynamic
}

proc cookit::gd::build-dynamic {} {
    cookit::buildMake all
}

proc cookit::gd::install-dynamic {} {
    cookit::buildMake install
}

proc cookit::gd::packageslist-dynamic {} {
    return [list]
}

