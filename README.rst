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


DESCRIPTION
===========

A Varnish 4.1 meta-director that layers three sub-directors internally in order to
provide deterministic load-balancing (affinity) behavior as well as
default-case and ultimate fail-over support so that requests can be handled by
the "most preferred" application server backend currently available.

CONTENTS
========

* Object director
* VOID director.add_backend(BACKEND, REAL)
* VOID director.add_exclusive_backend(BACKEND, STRING_LIST)
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


Description
  Create a new prehash meta-director.
Example
  ::

    vd_main = prehash.director();

.. _func_director.set_hash_header:

VOID director.set_hash_header(STRING_LIST)
------------------------------------------

Prototype
	VOID director.set_hash_header(STRING_LIST)

Description
  Set the header name whose value, if present, will be hashed in order to pick
  a backend. By default req.http.Host is used.
Example
  ::

    vd_main.set_hash_header("Publication");

.. _func_director.add_backend:

VOID director.add_backend(BACKEND, REAL)
----------------------------------------

Prototype
	VOID director.add_backend(BACKEND, REAL)

Description
  Add a new backend or director to the the prehash meta-director. New inbound
  requests that can be hashed (via Host header or header set w/
  set_hash_header()) are "pinned" to the backend closest to their hash value.
  This *is* deterministic based on the order in which backends were added (and
  its also a consistent hash, an unhealthy backend will *NOT* move hash values
  of other backends around). It is *NOT*, however, trivial to predetermine
  which hash values will be distributed to which backends; for that feature use
  `add_exclusive_backend()`. Note that weight should probably always be `1.0`
  otherwise the hash can become non-consistent.
Example
  ::

    vd_main.add_backend(vd_rr1.backend(), 1.0);

.. _func_director.add_exclusive_backend:

VOID director.add_exclusive_backend(BACKEND, STRING_LIST)
---------------------------------------------------------

Prototype
	VOID director.add_exclusive_backend(BACKEND, STRING_LIST)

Description
  Add a backend that will be *exclusively* used for a given hash value. The
  hash value is computed from the second argument. Note that normal backends
  can be reused as exclusive backends, although exclusives, if present, are
  always attempted first. Exclusive backends require an *exact* hash value
  match, not simply "nearest" as with `add_backend()`.
Example
  ::

    vd_main.add_exclusive_backend(vd_rr1.backend(), "www.myhost.com");

.. _func_director.add_lastresort_backend:

VOID director.add_lastresort_backend(BACKEND, REAL)
---------------------------------------------------

Prototype
	VOID director.add_lastresort_backend(BACKEND, REAL)

Description
  Add a last resort backends that will *only* be used if all normal backends
  (added via `add_backend()` are unhealthy). Multiple last resort backends can
  be added and will be used in round-robin order. Note that `std.healthy()`
  does *not* take into account lastresort backends when determining if the
  overall prehash meta-director is healthy, only if at least one of the
  backends added via `add_backend()` is healthy.
Example
  ::

    vd_main.add_lastresort_backend(lastresort, 1.0);

.. _func_director.finalize:

VOID director.finalize()
------------------------

Prototype
	VOID director.finalize()

Description
  Finalizes the entire meta-director. This should *only* be called after all backends have been added and no new backends can be added after this has been called. It is not necessary to call this method unless exclusive backends are actually in use.
Example
  ::

    vd_main.finalize();

.. _func_director.backend:

BACKEND director.backend()
--------------------------

Prototype
	BACKEND director.backend()

Description
  Returns a Varnish 4.1 backend director object that can be assigned to
  `req.backend_hint`. Final resolution of the actual real backend is deferred
  until just before the bereq is sent. Calling `std.healthy()` on the director
  returned by this method will be true if *any* non-exclusive backend is
  healthy _excluding_ last resorts.
Example
  ::

    set req.backend_hint = vd_main.backend();

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


Description
  Creates a pass-through director to make vcl standardization a bit easier.
  Pass-through directors always return the director or backend they encapsulate
  when their backend method is called.
Example
  ::

      backend ahost {
        .host = "127.0.0.1";
      }

      sub vcl_init {
        new s_ahost = prehash.passthru(ahost);
        new vdir = directors.round_robin();
        vdir.add_backend(s_ahost.backend());
      }

.. _func_passthru.backend:

BACKEND passthru.backend()
--------------------------

Prototype
	BACKEND passthru.backend()

Description
  Returns the actual backend or director that the passthru wraps.
