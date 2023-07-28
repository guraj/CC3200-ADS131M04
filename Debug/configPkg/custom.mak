## THIS IS A GENERATED FILE -- DO NOT EDIT
.configuro: .libraries,em4 linker.cmd package/cfg/empty_min_pem4.oem4

# To simplify configuro usage in makefiles:
#     o create a generic linker command file name 
#     o set modification times of compiler.opt* files to be greater than
#       or equal to the generated config header
#
linker.cmd: package/cfg/empty_min_pem4.xdl
	$(SED) 's"^\"\(package/cfg/empty_min_pem4cfg.cmd\)\"$""\"C:/Users/fhasd/workspace_v8/empty_min_CC3200_LAUNCHXL_TI_CC3200/Debug/configPkg/\1\""' package/cfg/empty_min_pem4.xdl > $@
	-$(SETDATE) -r:max package/cfg/empty_min_pem4.h compiler.opt compiler.opt.defs
