package main

/*
#cgo CFLAGS: -I./muxer
#cgo LDFLAGS: -L./muxer -lmuxer
#cgo LDFLAGS: -lavformat -lavcodec -lavutil -lm -lpthread -lswresample -lx264 -lbz2 -lz
#include "muxer/h264TsMuxer.h"
*/
import "C"
import (
	"fmt"
	"io/ioutil"
	"log"
	"unsafe"
)

func main() {
	C.av_register_all()
	var mgr C.struct_TsManager
	sname := C.CString("test.ts")
	defer C.free(unsafe.Pointer(sname))
	C.NewTsManagerInstsnce(&mgr, sname, 15, 640, 360, 400000)
	for ii := 1; ii != 2768; ii++ {
		fname := fmt.Sprintf("BackhandShotsAllEnglandOpenLow/frame%d.h264", ii)
		dat, err := ioutil.ReadFile(fname)
		if err != nil {
			log.Println(err)
			continue
		}
		size := len(dat)
		C.HandleReceiveFrameData(&mgr, (*C.uint8_t)(unsafe.Pointer(&dat[0])), C.int(size), C.int64_t((ii-1)*1000/15))
	}
	C.DeleteTsManager(&mgr)
}
