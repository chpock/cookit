# cookit - installer
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require cookit
package require https
package require tdom
package require vfs::tar

namespace eval ::cookit::install {

    variable binaries [expr { $::tcl_platform(platform) eq "windows" ? {
        cookit.exe  cookit-gui.exe  cookitA.exe  cookitA-gui.exe
        cookitU.exe cookitU-gui.exe cookitUA.exe cookitUA-gui.exe
        cookit-gui.com  cookitA-gui.com
        cookitU-gui.com cookitUA-gui.com

        cookit8.exe  cookit8-gui.exe  cookit8A.exe  cookit8A-gui.exe
        cookit8U.exe cookit8U-gui.exe cookit8UA.exe cookit8UA-gui.exe
        cookit8-gui.com  cookit8A-gui.com
        cookit8U-gui.com cookit8UA-gui.com
    } : {
        cookit  cookit-gui
        cookitU cookitU-gui

        cookit8  cookit8-gui
        cookit8U cookit8U-gui
    } }]

    variable console

}

proc ::cookit::install::follow_redirects { url } {
    while 1 {
        set token [http::geturl $url -validate 1]
        if { [http::ncode $token] == 302 && [incr count] < 16 } {
            set meta [::http::meta $token]
            set key [lsearch -inline -nocase [dict keys $meta] "location"]
            if { $key ne "" } {
                set url [dict get $meta $key]
                http::cleanup $token
                continue
            }
        }
        http::cleanup $token
        break
    }
    return $url
}

proc ::cookit::install::stage { msg } {

    variable last_stage_msg
    variable console

    if { $console } {
        if { [::cookit::is_tty] } {
            puts -nonewline stdout "\[..\] ${msg}..."
            flush stdout
        } else {
            puts -nonewline stdout "${msg}..."
        }
    }

    set last_stage_msg $msg

}

proc ::cookit::install::error { msg } {

    variable last_stage_msg
    variable console

    if { $console } {
        if { [::cookit::is_tty] } {
            puts stdout "\x0D\[ER\] ${last_stage_msg}: $msg"
        } else {
            puts stdout "${last_stage_msg}: ERROR - $msg"
        }
    } else {
        tk_messageBox -message "${last_stage_msg}: ERROR - $msg" \
            -title "Cookit installation error" \
            -type ok -icon error
    }

    cleanup

    exit 1

}

proc ::cookit::install::ok { { msg {} } } {

    variable last_stage_msg
    variable console

    if { $console } {
        if { [::cookit::is_tty] } {
            set msg [expr { $msg eq "" ? "     " : ": $msg" }]
            puts stdout "\x0D\[OK\] ${last_stage_msg}$msg"
        } else {
            set msg [expr { $msg eq "" ? "" : " - $msg" }]
            puts stdout "${last_stage_msg}: OK"
        }
    }

}

proc ::cookit::install::cleanup { } {
    if { [string match "installer.*" [file tail [info nameofexecutable]]] } {
        catch { file delete -force [info nameofexecutable] }
    }
}

proc ::cookit::install::success { action version home } {

    variable console

    set home [file nativename $home]


    if { $action eq "upgrade" } {

        set message "\nCookit in $home was successfully upgraded to version $version"

    } else {

        set message "\nCookit $version was successfully installed to $home"

        if { $::tcl_platform(platform) eq "windows" } {

            package require registry
            set path [registry get {HKEY_CURRENT_USER\Environment} Path]
            set path [split $path ";"]
            set path_exists 1

            if { [lsearch -nocase $path $home] == -1 } {
                lappend path $home
                set path [join $path ";"]
                if { [catch { registry set {HKEY_CURRENT_USER\Environment} Path $path }] } {
                    set path_exists 0
                }
            }

            append message "\n"

            if { $path_exists } {
                append message "Directory $home was successfully added to PATH for the current user. You can open a new command prompt and start using Cookit."
            } else {
                append message "Failed to add directory $home to PATH for current user."
            }

        } else {
            append message "\n"
            append message "\nPlease add this directory to your PATH environment variable."
        }
    }

    if { $console } {
        puts $message
    } else {
        tk_messageBox -message $message \
            -title "Cookit installation" \
            -type ok -icon info
    }

}

proc ::cookit::install::run { action args } {

    variable binaries
    variable console

    set console [catch { package require Tk } err]
    if { !$console } {
        # hide toplevel window
        wm withdraw .
    }

    ############################################################
    stage "Checking for the installation directory"
    ############################################################

    if { $action eq "install" } {
        if { [catch {
            if { $::tcl_version < 9.0 } {
                set home [file normalize ~]
            } else {
                set home [file home]
            }
        } err] } {
            error "could not detect user home directory: $err"
        } elseif { $home eq "" } {
            error "could not detect user home directory: empty result"
        }

        set home [file join $home ".cookit"]
    } else {
        set home [file normalize [file dirname [info nameofexecutable]]]
    }

    ok $home

    ############################################################
    stage "Checking for the latest Cookit release"
    ############################################################

    set url "https://api.github.com/repos/chpock/cookit/releases/latest"

    if { [catch { http::geturl [follow_redirects $url] } token] } {
        error "HTTP request failed: $token"
    }
    if { [http::ncode $token] != 200 } {
        error "HTTP status [http::ncode $token] != 200\nResponse body: [http::data $token]"
    }
    if { [catch { dom parse -json [http::data $token] } dom] } {
        error "Could not parse JSON: $dom\nResponse body: [http::data $token]"
    }
    http::cleanup $token

    set release [$dom asTclValue]
    $dom delete

    set version [dict get $release name]

    ok $version

    if { $action eq "upgrade" } {

        stage "Checking the current version of Cookit"

        set version_current "v[::cookit::pkgconfig get package-version]"

        if { $version eq $version_current } {

            ok "$version_current - an upgrade is not required."

            if { !$console } {
                tk_messageBox -message "The latest available release of Cookit $version is the same as the current version.\nNo update is required." \
                    -title "Cookit installation" \
                    -type ok -icon info
            }

            return

        } else {
            ok "$version_current - an upgrade is required."
        }

    }

    ############################################################
    set platform [::cookit::pkgconfig get platform]
    stage "Checking for an achive for platform '$platform'"
    ############################################################

    set mask "*.${platform}.tar.gz"

    foreach asset [dict get $release assets] {
        set url [dict get $asset browser_download_url]
        if { [string match $mask $url] } break
        unset url
    }

    if { ![info exists url] } {
        error "Could not find an asset with mask '$mask'"
    }

    ok $url

    ############################################################
    stage "Downloading the release"
    ############################################################

    set chan [file tempfile archive]
    fconfigure $chan -translation binary

    set stream [zlib stream gunzip]
    chan push $chan [list apply {{ stream cmd handle {data {}} } {
        switch -exact -- $cmd {
            initialize { return [list initialize finalize write] }
            finalize   { return }
            write      { $stream put $data }
        }
        return [$stream get]
    }} $stream]

    if { [catch { http::geturl [follow_redirects $url] -channel $chan } token] } {
        close $chan
        file delete -force $archive
        error "HTTP request failed: $token"
    }

    chan pop $chan

    if { [http::ncode $token] != 200 } {
        close $chan
        file delete -force $archive
        error "HTTP status [http::ncode $token] != 200\nResponse body: [http::data $token]"
    }
    http::cleanup $token

    ok

    ############################################################
    stage "Extracting Cookit binaries"
    ############################################################

    if { [catch { file mkdir $home } err] } {
        close $chan
        file delete -force $archive
        error "could not create the home directory '$home': $err"
    }

    seek $chan 0

    if { [catch {

        vfs::tar::TOC $chan sb "::vfs::tar::$chan.toc"
        vfs::filesystem mount $archive [list ::vfs::tar::handler $chan]
        vfs::RegisterMount $archive [list ::vfs::tar::Unmount $chan]

        set subdir [glob -directory $archive -type d -nocomplain *]

    } err] } {
        close $chan
        file delete -force $archive
        error "error reading archive: $err"
    }

    if { [llength $subdir] != 1 } {
        vfs::unmount $archive; # This will close the channel also
        file delete -force $archive
        error "malformed archive, only 1 subdirectory expected, but got [llength $subdir] subdirectories"
    }

    set unpacked [list]

    foreach bin $binaries {
        set tempbin [file join $home "${bin}.pending"]
        set archivebin [file join $archive $subdir $bin]
        if { [catch { file copy -force $archivebin $tempbin } err] } {
            close $chan
            file delete -force $archive
            foreach bin $unpacked {
                catch { file delete -force $bin }
            }
            error "error while copying '$archivebin' to '$tempbin': $err"
        }
        lappend unpacked $tempbin
    }

    vfs::unmount $archive; # This will close the channel also
    file delete -force $archive

    ok

    ############################################################
    stage "Copying Cookit binaries"
    ############################################################

    set backuped [list]

    foreach bin $binaries {
        set bin [file join $home $bin]
        if { ![file exists $bin] } {
            continue
        }
        set backup "${bin}.temp"
        if { [catch { file rename -force $bin $backup } err] } {
            foreach bin $unpacked {
                catch { file delete -force $bin }
            }
            foreach bin $backuped {
                catch { file rename -force $bin [file rootname $bin] }
            }
            error "error while renaming '$bin' to '$backup': $err"
        }
        lappend backuped $backup
    }

    foreach bin $unpacked {
        if { [catch { file rename -force $bin [file rootname $bin] } err] } {
            foreach bin $unpacked {
                catch { file delete -force $bin }
            }
            foreach bin $backuped {
                catch { file rename -force $bin [file rootname $bin] }
            }
            error "error while renaming '$bin' to '[file rootname $bin]': $err"
        } else {
            ::cookit::set_exec_perms [file rootname $bin]
        }
    }

    foreach bin $backuped {
        catch { file delete -force $bin }
    }

    ok

    success $action $version $home

    cleanup

}

package provide cookit::install 1.0.0
