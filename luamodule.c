#include <Python.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h> 

static PyObject* LuaError;

static PyObject*
lua_eval(PyObject* self, PyObject* args, PyObject* keywds)
{
    const char* command;
    //PyObject* tgs = PyEval_GetGlobals();
    PyObject* gs = PyEval_GetGlobals();
    PyObject* ls = NULL;
    static char* kwlist[] = {"expression", "globals", "locals", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|OO", kwlist, &command, &gs, &ls))
        return NULL;
    if (gs == Py_None)
        gs = PyEval_GetGlobals();
    if (ls == Py_None)
        ls = NULL;
    lua_State *L;
    L = luaL_newstate();
    luaL_openlibs(L);
    //так как и globals, и locals доступны всецело в lua, то можно их добавить в lua environment
    //lua неявно кастует все строковые переменные к типу выражения (например a = "5", c = a + 10 -> c = 15)
    //"прокинем" в lua как строки
    PyObject *gskey, *gsvalue;
    Py_ssize_t gspos = 0;
    if (gs != NULL && PyDict_Size(gs) != 0)
    {
        while (PyDict_Next(gs, &gspos, &gskey, &gsvalue))
        {
            PyObject* str_key = PyObject_Repr(gskey);
            PyObject* str_value = PyObject_Repr(gsvalue);
            PyObject* pyStrKey = PyUnicode_AsEncodedString(str_key, "utf-8", "Error ~");
            PyObject* pyStrValue = PyUnicode_AsEncodedString(str_value, "utf-8", "Error ~");
            const char* k = PyBytes_AS_STRING(pyStrKey);
            int len = strlen(k);
            char* realk = (char*) malloc(len - 2);
            memcpy(realk, k + 1, len - 2);
            realk[len - 2] = '\0';
            const char* v = PyBytes_AS_STRING(pyStrValue);
            //check if it is string constant from python
            len = strlen(v);
            if (v[0] == '\'' && v[len - 1] == '\'')
            {
                char* realv = (char*) malloc(len - 2);
                memcpy(realv, v + 1, len - 2);
                realv[len - 2] = '\0';
                lua_pushstring(L, realv);
            }
            else
                lua_pushstring(L, v);
            lua_setglobal(L, realk);
        }
    }
    PyObject *lskey, *lsvalue;
    Py_ssize_t lspos = 0;
    if (ls != NULL && PyDict_Size(ls) != 0)
    {
        while (PyDict_Next(ls, &lspos, &lskey, &lsvalue))
        {
            PyObject* str_key = PyObject_Repr(lskey);
            PyObject* str_value = PyObject_Repr(lsvalue);
            PyObject* pyStrKey = PyUnicode_AsEncodedString(str_key, "utf-8", "Error ~");
            PyObject* pyStrValue = PyUnicode_AsEncodedString(str_value, "utf-8", "Error ~");
            const char* k = PyBytes_AS_STRING(pyStrKey);
            int len = strlen(k);
            char* realk = (char*) malloc(len - 2);
            memcpy(realk, k + 1, len - 2);
            realk[len - 2] = '\0';
            const char* v = PyBytes_AS_STRING(pyStrValue);
            //check if it is string constant from python
            len = strlen(v);
            if (v[0] == '\'' && v[len - 1] == '\'')
            {
                char* realv = (char*) malloc(len - 2);
                memcpy(realv, v + 1, len - 2);
                realv[len - 2] = '\0';
                lua_pushstring(L, realv);
            }
            else
            {
                if (strcmp(v, "True") || strcmp(v, "TRUE"))
                {
                    lua_pushboolean(L, 1);
                }
                else
                {
                    if (strcmp(v, "False") || strcmp(v, "False"))
                    {
                        lua_pushboolean(L, 0);
                    }
                    else
                        lua_pushstring(L, v);
                }
            }
            lua_setglobal(L, realk);
        }
    }
    int error1 = luaL_loadstring(L, command);
    if (error1)
    {
        PyErr_SetString(LuaError, lua_tostring(L, -1));
        return NULL;
    }
    else
    {
        int error2 = lua_pcall(L, 0, 1, 0);
        if (error2)
        {
            PyErr_SetString(LuaError, lua_tostring(L, -1));
            return NULL;
        }
        else
        {
            //получаем новые значения globals
            PyObject *ngskey, *ngsvalue;
            Py_ssize_t ngspos = 0;
            if (gs != NULL && PyDict_Size(gs) != 0)
            {
                while (PyDict_Next(gs, &ngspos, &ngskey, &ngsvalue))
                {
                    PyObject* nstr_key = PyObject_Repr(ngskey);
                    PyObject* npyStrKey = PyUnicode_AsEncodedString(nstr_key, "utf-8", "Error ~");
                    const char* nk = PyBytes_AS_STRING(npyStrKey);
                    int len = strlen(nk);
                    char* realk = (char*) malloc(len - 2);
                    memcpy(realk, nk + 1, len - 2);
                    realk[len - 2] = '\0';
                    //у нас ограничены типы, а это системные глобалы других типов
                    PyObject* mainPy = PyImport_ImportModule("__main__");
                    PyObject* ch = PyObject_GetAttrString(mainPy, realk);
                    if(!PyLong_Check(ch) && !PyBool_Check(ch) && !PyFloat_Check(ch) && !PyUnicode_Check(ch))
                    {
                        lua_remove(L, -1);
                        continue;
                    }
                    lua_getglobal(L, realk);
                    if (lua_isnone(L, - 1))
                    {
                        PyDict_SetItem(gs, ngskey, Py_None);
                        lua_remove(L, -1);
                        continue;
                    }
                    if (lua_isnumber(L, -1))
                    {
                        double ret = lua_tonumber(L, -1);
                        if (ret == (int) ret)
                            PyDict_SetItem(gs, ngskey, Py_BuildValue("i", (int) ret));
                        else
                            PyDict_SetItem(gs, ngskey, Py_BuildValue("d", ret));
                        lua_remove(L, -1);
                        continue;
                     }
                     if (lua_isboolean(L, -1))
                        {
                            int ret = lua_toboolean(L, -1);
                            PyDict_SetItem(gs, ngskey, Py_BuildValue("i", ret));
                            lua_remove(L, -1);
                            continue;
                        }
                     if(lua_isstring(L, -1))
                     {
                        const char* ret = lua_tostring(L, -1);
                        PyDict_SetItem(gs, ngskey, Py_BuildValue("s", ret));
                        lua_remove(L, -1);
                        continue;
                     }
                }
                //получаем новые значения locals
                PyObject *nlskey, *nlsvalue;
                Py_ssize_t nlspos = 0;
                if (ls != NULL && PyDict_Size(ls) != 0)
                {
                    while (PyDict_Next(ls, &nlspos, &nlskey, &nlsvalue))
                    {
                        PyObject* nstr_key = PyObject_Repr(nlskey);
                        PyObject* npyStrKey = PyUnicode_AsEncodedString(nstr_key, "utf-8", "Error ~");
                        const char* nk = PyBytes_AS_STRING(npyStrKey);
                        int len = strlen(nk);
                        char* realk = (char*) malloc(len - 2);
                        memcpy(realk, nk + 1, len - 2);
                        realk[len - 2] = '\0';
                        lua_getglobal(L, realk);
                        if (lua_isnone(L, - 1))
                        {
                            PyDict_SetItem(ls, nlskey, Py_None);
                            lua_remove(L, -1);
                            continue;
                        }
                        if (lua_isnumber(L, -1))
                        {
                            double ret = lua_tonumber(L, -1);
                            if (ret == (int) ret)
                                PyDict_SetItem(ls, nlskey, Py_BuildValue("i", (int) ret));
                            else
                                PyDict_SetItem(ls, nlskey, Py_BuildValue("d", ret));
                            lua_remove(L, -1);
                            continue;
                        }
                        if (lua_isboolean(L, -1))
                            {
                                int ret = lua_toboolean(L, -1);
                                PyDict_SetItem(ls, nlskey, Py_BuildValue("i", ret));
                                lua_remove(L, -1);
                                continue;
                            }
                        if(lua_isstring(L, -1))
                        {
                            const char* ret = lua_tostring(L, -1);
                            PyDict_SetItem(ls, nlskey, Py_BuildValue("s", ret));
                            lua_remove(L, -1);
                            continue;
                        }
                    }
                }
            }
            if (lua_isnumber(L, -1))
            {
                double ret = lua_tonumber(L, -1);
                if (ret == (int) ret)
                    return Py_BuildValue("i", (int) ret);
                else
                    return Py_BuildValue("d", ret);
            }
            if (lua_isboolean(L, -1))
            {
                int ret = lua_toboolean(L, -1);
                return Py_BuildValue("i", ret);
            }
            if (lua_isstring(L, -1))
            {
                const char* ret = lua_tostring(L, -1);
                return Py_BuildValue("s", ret);
            }
        }
    }
    lua_close(L);
    Py_INCREF(Py_None);
    return Py_None;
}

static char eval_docs[] =
    "eval(expression: str, globals: dict = None, locals: dict = None): execute lua string\n";

static PyMethodDef LuaMethods[] = {
    {"eval", (PyCFunction) lua_eval, METH_VARARGS|METH_KEYWORDS, eval_docs},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef luamodule = {
   PyModuleDef_HEAD_INIT,
   "lua",   /* name of module */
   NULL, /* module documentation, may be NULL */
   -1,       /* size of per-interpreter state of the module,
                or -1 if the module keeps state in global variables. */
   LuaMethods
};

PyMODINIT_FUNC
PyInit_lua(void)
{
    PyObject* m = PyModule_Create(&luamodule);
    LuaError = PyErr_NewException("lua.error", NULL, NULL);
    Py_INCREF(LuaError);
    PyModule_AddObject(m, "error", LuaError);
    return m;
}