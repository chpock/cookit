namespace eval cookit::tclgd {}

cookit::partRegister tclgd "TLS: OpenSSL Tcl Extension"

proc cookit::tclgd::retrievesource {} {
}

proc cookit::tclgd::parameters {version} {
    return [list \
        provides {} depends {gd "" tcl ""} \
        buildmodes {dynamic} \
        ]
}

proc cookit::tclgd::initialize-dynamic {version} {
}

proc cookit::tclgd::configure-dynamic {} {
    set additional [list]

    cookit::addToPath \
        [list [file join [cookit::getInstallDynamicDirectory] bin]] \
        [list [file join [cookit::getInstallDynamicDirectory] lib]] \
        {
        catch {exec gdlib-config}
        cookit::buildConfigure -sourcepath relative -with-tcl relative \
            -mode dynamic
    }
}

proc cookit::tclgd::build-dynamic {} {
    cookit::buildMake tclgd.o tclgdtcl.o tclgdio.o
}

proc cookit::tclgd::install-dynamic {} {
}

proc cookit::tclgd::packageslist-dynamic {} {
    set packages [list]
    return $packages
}
