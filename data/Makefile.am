## data/Makefile.am 

##if INSTALL_INIT_TEST
## Fill in details of btnx init script
$(PACKAGE):
	@sed -e 's:<PACKAGE>:$(PACKAGE):' \
		-e 's:<EXEC_PATH>:$(sbindir)/$(PACKAGE):' \
		-e 's:<SCRIPT_NAME>:$(initdir)/$(PACKAGE):' \
		-e 's:<SYSLOG>:$(arg_syslog):' \
		<btnx-init-template >$(PACKAGE)
initexec_PROGRAMS = $(PACKAGE)
initexecdir = $(initdir)
EXTRA_PROGRAMS = init
##endif

config_DATA = events

EXTRA_DIST = $(PACKAGE)-init-template

uninstall-local:
	## remove files, possible left-overs
	-$(RM) -rf $(DESTDIR)$(configdir)
	-$(RM) -f $(DESTDIR)$(pid_path)/$(PACKAGE).pid
	-$(RM) -f $(DESTDIR)/etc/udev/rules.d/99-btnx.rules
if INSTALL_INIT_TEST
	## unregister/uninstall init script
	@echo "Unregistering init script"
	@echo "$(init_del_cmd)"
	@$(init_del_cmd)
	-$(RM) -f $(DESTDIR)$(initdir)/$(PACKAGE)

install-exec-hook:
	## register init script
	@echo "Registering init script"
	@echo "$(init_add_cmd)"
	@$(init_add_cmd)

clean-local:
	## clean the generated init script
	-$(RM) -f $(PACKAGE)
endif

