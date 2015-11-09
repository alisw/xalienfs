use ExtUtils::MakeMaker;
WriteMakefile(
	      'CC'                => 'g++',
	      'LD'                => 'g++',
	      'NAME'              => 'gapiUI',
	      'VERSION'           => '1.00',
	      'OBJECT'            => 'gapiUI_wrap.o -L../.libs/ -lgapiUI',
	      'INC'               => '-I /usr/kerberos/include -I $(PERLPREFIX)/include ',
	      'DEFINE'            => '-D__SWIG__',
	      'PMLIBDIRS'         => ['gapi'],
	      );
