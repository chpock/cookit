# cookit - postpone file operations on Windows platform
#
# Copyright (C) 2024 Konstantin Kushnir <chpock@gmail.com>
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require cookit

namespace eval ::cookit::windows {}
namespace eval ::cookit::windows::postpone {

    variable COMSPEC
    variable actions

}

proc ::cookit::windows::postpone::add { action args } {
    variable actions
    lappend actions $action $args
}

proc ::cookit::windows::postpone::on_exit { args } {

    package require twapi_process

    variable actions
    variable COMSPEC

    if { ![info exists actions] || ![llength $actions] } {
        return
    }

    set batch [split {
        @echo off
        setlocal

        set "max_attempts=30"
        goto :main

        :rename_file
        set "source=%~1"
        set "destination=%~2"
        set /a "attempt=0"

        :rename_retry
        ren "%source%" "%destination%" >nul 2>&1
        if exist "%source%" (
            set /a attempt+=1
            if %attempt% lss %max_attempts% (
                timeout /t 1 /nobreak >nul
                goto :rename_retry
            )
        )
        exit /b 0

        :delete_file
        set "source=%~1"
        set /a "attempt=0"

        :delete_retry
        del "%source%" >nul 2>&1
        if exist "%source%" (
            set /a attempt+=1
            if %attempt% lss %max_attempts% (
                timeout /t 1 /nobreak >nul
                goto :delete_retry
            )
        )
        exit /b 0

        :main
    } \n]

    foreach { action arguments } $actions {
        switch -exact -- $action {
            "delete" {
                set file [lindex $arguments 0]
                set dir  [file nativename [file dirname $file]]
                set file [file tail $file]
                lappend batch "pushd \"$dir\" && call :delete_file \"$file\" && popd"
            }
            "rename" {
                set file [lindex $arguments 0]
                set dir  [file nativename [file dirname $file]]
                set file [file tail $file]
                set dest [file tail [lindex $arguments 1]]
                lappend batch "pushd \"$dir\" && call :rename_file \"$file\" \"$dest\" && popd"
            }
            "delete_empty" {
                set file [lindex $arguments 0]
                set dir  [file nativename [file dirname $file]]
                set file [file tail $file]
                lappend batch "pushd \"$dir\" && rd /q \"$file\" >nul 2>&1 && popd"
            }
        }
    }

    lappend batch {*}[split {
        del "%~f0"
        exit /b 0
    } \n]

    set batch [lmap x $batch { string trim $x }]

    while 1 {
        close [file tempfile tempname cookit_postpone]
        file delete -force $tempname
        append tempname ".bat"
        if { ![file exists $tempname] } break
    }

    set chan [open $tempname w]
    fconfigure $chan -translation crlf -encoding [encoding system]

    foreach line $batch {
        puts $chan $line
    }

    close $chan

    catch {
        ::twapi::create_process $COMSPEC -cmdline "\"$COMSPEC\" /C \"[file nativename $tempname]\"" \
            -detached 0 -feedbackcursoroff 1 -newprocessgroup 1 -showwindow hidden -newconsole 1
    }

}

proc ::cookit::windows::postpone::init {} {

    variable COMSPEC

    if { [info exists ::env(COMSPEC)] } {
        set COMSPEC $::env(COMSPEC)
    } elseif { [info exists ::env(WINDIR)] && [file exists [set path [file join $::env(WINDIR) System32 cmd.exe]]] } {
        set COMSPEC $path
    } elseif { [info exists ::env(SYSTEMROOT)] && [file exists [set path [file join $::env(WINDIR) System32 cmd.exe]]] } {
        set COMSPEC $path
    } else {
        return -code error "environment variable COMSPEC is not defined"
    }

    trace add execution exit enter ::cookit::windows::postpone::on_exit

}

::cookit::windows::postpone::init

package provide cookit::windows::postpone 1.0.0
