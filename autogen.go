package main

import "fmt"
import "os"
import "os/exec"


func checkFile(file string) bool {
	if _, err := os.Stat(file); err == nil {
		  return true
    }
    return false
}

func checkFiles(files []string) bool {
    for _, file := range files {
        if checkFile(file) == false {
            return false
        }
    }
    return true
}

func check_single(key string, file string) string {
    if checkFile(file) == false {
        return ""
    }
    return key
}

func check_pkgconf(key string, pkg string) string {
    cmd := exec.Command("pkg-config", "--cflags-only-I", pkg)
    out, err := cmd.Output()
    if err != nil {
        return ""
    }
    if len(string(out)) == 0 {
        return ""
    }
    return key
}

func main() {
    pkgs := make(map[string]string)
    pkgs["ffmpeg"] = "libavformat"
    pkgs["gstreamer"] = "gstreamer"
    pkgs["srtp"] = "opensrtp"
    pkgs["widgets"] = "Qt5Gui"
    
    m := make(map[string]string)
    m["x264"] = "/usr/include/x264.h"
    m["x11"] = "/usr/include/X11/extensions/Xvlib.h"
    m["alsa"] = "/usr/include/alsa/control.h"
    m["mad"] = ""
    m["srtp"] = "/usr/include/srtp/srtp.h"

    config := make([]string, 0)
    
    //static config options
    config = append(config, "staticlib")
    
    for ch := range m {
        config = append(config, check_single(ch, m[ch]))
    }
    
    for ch := range pkgs {
        config = append(config, check_pkgconf(ch, pkgs[ch]))
    }
    
    fmt.Println("INSTALL_PREFIX=$$OUT_PWD/../../")
    fmt.Println("DEFINES += DEBUG INFO LOG LOGV DEBUG_TIMING")
    fmt.Printf("CONFIG += ")
    for _, c := range config {
        if len(c) > 0 {
            fmt.Printf("%s ", c)
        }
    }
    fmt.Println()
    //fmt.Println(config)
}
