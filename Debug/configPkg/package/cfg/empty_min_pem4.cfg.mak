# invoke SourceDir generated makefile for empty_min.pem4
empty_min.pem4: .libraries,empty_min.pem4
.libraries,empty_min.pem4: package/cfg/empty_min_pem4.xdl
	$(MAKE) -f C:\Users\fhasd\workspace_v8\empty_min_CC3200_LAUNCHXL_TI_CC3200/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\fhasd\workspace_v8\empty_min_CC3200_LAUNCHXL_TI_CC3200/src/makefile.libs clean

