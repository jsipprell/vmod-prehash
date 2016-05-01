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

* :ref:`obj_director`
* :ref:`func_director.add_backend`
* :ref:`func_director.add_hashed_backend`
* :ref:`func_director.add_lastresort_backend`
* :ref:`func_director.backend`
* :ref:`func_director.hash`
* :ref:`func_director.set_hash_header`

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
.. _func_director.backend:

BACKEND director.backend()
--------------------------

Prototype
	BACKEND director.backend()
.. _func_director.hash:

BACKEND director.hash(STRING_LIST)
----------------------------------

Prototype
	BACKEND director.hash(STRING_LIST)
