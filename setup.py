from distutils.core import setup, Extension

module1 = Extension('lua',
                    sources = ['luamodule.c']
		   )
setup (name = 'lua',
       version = '1.0',
       description = 'lua execution module',
       ext_modules = [module1])
