..
.. NB:  This file is machine generated, DO NOT EDIT!
..
.. Edit vmod.vcc and run make instead
..

.. role:: ref(emphasis)

.. _vmod_prehash(3):

============
vmod_prehash
============

----------------------------------------------------------
Varnish Prehash Director w/ Overrides & Random Fallthrough
----------------------------------------------------------

:Manual section: 3

SYNOPSIS
========

import prehash [from "path"] ;


CONTENTS
========

* Object director
* VOID director.add_backend(BACKEND, REAL)
* VOID director.add_hashed_backend(BACKEND, STRING_LIST)
* VOID director.add_lastresort_backend(BACKEND, REAL)
* BACKEND director.backend()
* VOID director.finalize()
* BACKEND director.hash(STRING_LIST)
* BACKEND director.self()
* VOID director.set_hash_header(STRING_LIST)
* Object passthru
* BACKEND passthru.backend()

.. _obj_director:

Object director
===============

.. _func_director.set_hash_header:

VOID director.set_hash_header(STRING_LIST)
------------------------------------------

Prototype
	VOID director.set_hash_header(STRING_LIST)
.. _func_director.add_backend:

VOID director.add_backend(BACKEND, REAL)
----------------------------------------

Prototype
	VOID director.add_backend(BACKEND, REAL)
.. _func_director.add_hashed_backend:

VOID director.add_hashed_backend(BACKEND, STRING_LIST)
------------------------------------------------------

Prototype
	VOID director.add_hashed_backend(BACKEND, STRING_LIST)
.. _func_director.add_lastresort_backend:

VOID director.add_lastresort_backend(BACKEND, REAL)
---------------------------------------------------

Prototype
	VOID director.add_lastresort_backend(BACKEND, REAL)
.. _func_director.finalize:

VOID director.finalize()
------------------------

Prototype
	VOID director.finalize()
.. _func_director.backend:

BACKEND director.backend()
--------------------------

Prototype
	BACKEND director.backend()
.. _func_director.self:

BACKEND director.self()
-----------------------

Prototype
	BACKEND director.self()
.. _func_director.hash:

BACKEND director.hash(STRING_LIST)
----------------------------------

Prototype
	BACKEND director.hash(STRING_LIST)

.. _obj_passthru:

Object passthru
===============

.. _func_passthru.backend:

BACKEND passthru.backend()
--------------------------

Prototype
	BACKEND passthru.backend()
