## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4


bin_PROGRAMS = gshell gbbox gps gsubmit gcmd gisonline gprepare gstage gcp

INCLUDES = -I$(top_srcdir)/base -I$(top_srcdir)/client/gclientlib
gshell_SOURCES = gshell.cxx 
gshell_LDADD = -L../gclientlib -lgapiUI


gbbox_SOURCES = gbbox.cxx
gbbox_LDADD = -L../gclientlib -lgapiUI
gbbox_LDFLAGS = 

gps_SOURCES = gps.cxx
gps_LDADD = -L../gclientlib -lgapiUI

gcmd_SOURCES = gcmd.cxx

gsubmit_SOURCES = gsubmit.cxx
gsubmit_LDADD = -L../gclientlib -lgapiUI

gisonline_SOURCES = gisonline.cxx
gisonline_LDADD = -L../gclientlib -lgapiUI

gprepare_SOURCES = gprepare.cxx
gprepare_LDADD = -L../gclientlib -lgapiUI


gstage_SOURCES = gstage.cxx
gstage_LDADD = -L../gclientlib -lgapiUI

gcp_SOURCES = gcp.cxx
gcp_LDADD = -L../gclientlib -lgapiUI

EXTRA_DIST = scripts/$(GPREFIX)_apiservice-bootstrap \
        scripts/env_gbbox.sh \
        scripts/$(GPREFIX)_bash_completion.gShell \
        scripts/$(GPREFIX)_cat \
        scripts/$(GPREFIX)_run \
        scripts/$(GPREFIX)_cd \
        scripts/$(GPREFIX)_cp \
        scripts/$(GPREFIX)_edit \
        scripts/$(GPREFIX)_less \
        scripts/$(GPREFIX)_more \
        scripts/$(GPREFIX)_Shell\
	scripts/$(GPREFIX)sh\
	scripts/gprefix_Shell\
	scripts/gprefixsh \
	scripts/gprefix_apiservice-bootstrap\
	scripts/gprefix_bash_completion.gShell\
	scripts/gprefix-token-init\
	scripts/gprefix-token-info\
	scripts/gprefix-token-destroy\
	scripts/gprefix_packages\
	scripts/gprefix_cat\
	scripts/gprefix_run\
	scripts/gprefix_cd\
	scripts/gprefix_cp\
	scripts/gprefix_edit\
	scripts/gprefix_less\
	scripts/gprefix_more\
	scripts/gprefix_rm.pl


# these are the grid commands for which dynamic links to the gbbox will be
# created
GCOMMANDS = connect \
            ls \
	    mv \
	    rm \
            pwd \
	    kill \
            top \
            df \
	    whereis \
            find \
	    resubmit \
	    mkdir \
            rmdir \
            purge \
            erase \
	    whoami \
	    find \
	    queue \
	    spy \
	    mirror \
	    who \
	    addTag\
	    addTagValue\
	    addTrigger\
	    showTags\
	    showTagValue\
	    showAllTagValue\
	    showTrigger\
	    removeTag\
	    removeTagValue\
	    removeTrigger\
	    updateTagValue\
	    chgroup\
	    guid2lfn\
	    lfn2guid\
	    partitions\
	    services\
	    registerOutput


# these are the scripts to be installed into bin
GSCRIPTBIN = _apiservice-bootstrap \
	_Shell \
	_cat \
	_run \
	_cd \
	_cp \
	_edit \
	_less \
	_more \
	sh \
	\-token-init \
	\-token-info \
	\-token-destroy 



# these are the scripts to be installed under etc
GSCRIPTETC = _bash_completion.gShell 


## TODO: the symbolic link mechanism is not really portable!
install-exec-local:
	gcc -v 2>&1 | grep "gcc version" | awk '{print $$3}' | cut -d "." -f1,2 > gapi-gcc-version
	mkdir -p $(DESTDIR)/$(prefix)/etc/;
	cd $(DESTDIR)/$(bindir); for name in $(GCOMMANDS); do ln -f -s gbbox $(GPREFIX)_$$name;done
	cd $(srcdir)/scripts; for name in $(GSCRIPTBIN); do cat gprefix$$name | sed s/GPREFIX/$(GPREFIX)/g | sed s/GSHELL_VERSION/$(VERSION)x/g > $(GPREFIX)$$name ;done
	cd $(srcdir)/scripts; for name in $(GSCRIPTETC); do cat gprefix$$name | sed s/GPREFIX/$(GPREFIX)/g | sed s/GSHELL_VERSION/$(VERSION)x/g > $(GPREFIX)$$name ;done
	for name in $(GSCRIPTBIN); do $(INSTALL) -m 755 $(srcdir)/scripts/$(GPREFIX)$$name $(DESTDIR)/$(bindir) ; done
	for name in $(GSCRIPTETC); do $(INSTALL) -m 644 $(srcdir)/scripts/$(GPREFIX)$$name $(DESTDIR)/$(prefix)/etc/ ; done
	$(INSTALL) -m 644 gapi-gcc-version $(DESTDIR)/$(bindir)/../etc/
	cd $(DESTDIR)/$(bindir); ln -f -s gbbox g$(GPREFIX)   
	cd $(DESTDIR)/$(bindir); ln -f -s gps $(GPREFIX)_ps
	cd $(DESTDIR)/$(bindir); ln -f -s gisonline $(GPREFIX)_isonline
	cd $(DESTDIR)/$(bindir); ln -f -s gprepare $(GPREFIX)_prepare
	cd $(DESTDIR)/$(bindir); ln -f -s gstage $(GPREFIX)_stage
	cd $(DESTDIR)/$(bindir); ln -f -s gcmd $(GPREFIX)_cmd
	cd $(DESTDIR)/$(bindir); ln -f -s gsubmit $(GPREFIX)_submit
	$(INSTALL) -m 755 $(srcdir)/scripts/env_gbbox.sh $(DESTDIR)/$(prefix)/etc/;
	pwd; cd $(srcdir); cp -Rv share $(DESTDIR)/$(prefix)/

bundle-lib-local:
	thisdir=$$PWD; mkdir lib; cp `ldd $$thisdir/../gclientlib/.libs/libgapiUI.so | grep -v "=> /lib" | awk '{print $$3}' ` lib; tar cvzf gshell-lib.tgz lib;rm -rf lib; 


uninstall-local:
	for name in $(GCOMMANDS); do rm -f $(bindir)/$(GPREFIX)_$$name;done
	for name in $(GSCRIPTBIN); do rm -f $(bindir)/$(GPREFIX)_$$name;done
	for name in $(GSCRIPTETC); do rm -f $(bindir)/../etc/$(GPREFIX)_$$name;done
	rm -f $(bindir)/../share/
	rm -f $(bindir)/g$(GPREFIX)
	rm -f $(bindir)/$(GPREFIX)_ps
	rm -f $(bindir)/$(GPREFIX)_submit
	rm -f $(bindir)/../etc/env_gbbox.sh
