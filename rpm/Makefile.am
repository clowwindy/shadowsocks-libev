clean-local:
	-rm -rf BUILD BUILDROOT RPMS SRPMS SPECS/*.spec SOURCES/*.tar SOURCES/*.gz
srpm:
	-./genrpm.sh -S
rpm:
	-./genrpm.sh

.PHONY: srpm rpm
