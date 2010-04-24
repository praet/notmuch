.. notmuch documentation master file, created by
   sphinx-quickstart on Tue Feb  2 10:00:47 2010.

.. currentmodule:: notmuch

Welcome to :mod:`notmuch`'s documentation
===========================================

The :mod:`notmuch` module provides an interface to the `notmuch <http://notmuchmail.org>`_ functionality, directly interfacing to a shared notmuch library.
Within :mod:`notmuch`, the classes :class:`Database`, :class:`Query` provide most of the core functionality, returning :class:`Threads`, :class:`Messages` and :class:`Tags`.

.. moduleauthor:: Sebastian Spaeth <Sebastian@SSpaeth.de>

:License: This module is covered under the GNU GPL v3 (or later).

This page contains the main API overview of notmuch |release|. 

Notmuch can be imported as::

 import notmuch

or::

 from notmuch import Query,Database

More information on specific topics can be found on the following pages:

.. toctree::
   :maxdepth: 1

   notmuch   

:mod:`notmuch` -- The Notmuch interface
=================================================

.. automodule:: notmuch

:todo: Document nmlib,STATUS

:class:`notmuch.Database` -- The underlying notmuch database
---------------------------------------------------------------------

.. autoclass:: notmuch.Database([path=None[, create=False[, mode=MODE.READ_ONLY]]])

   .. automethod:: create

   .. automethod:: open(path, status=MODE.READ_ONLY)

   .. automethod:: get_path

   .. automethod:: get_version

   .. automethod:: needs_upgrade

   .. automethod:: upgrade

   .. automethod:: get_directory

   .. automethod:: add_message

   .. automethod:: remove_message

   .. automethod:: find_message

   .. automethod:: get_all_tags

   .. automethod:: create_query

   .. note:: :meth:`create_query` was broken in release
             0.1 and is fixed since 0.1.1.

   .. attribute:: Database.MODE

     Defines constants that are used as the mode in which to open a database.

     MODE.READ_ONLY
       Open the database in read-only mode

     MODE.READ_WRITE
       Open the database in read-write mode

   .. autoattribute:: db_p

:class:`notmuch.Query` -- A search query
-------------------------------------------------

.. autoclass:: notmuch.Query

   .. automethod:: create

   .. attribute:: Query.SORT

     Defines constants that are used as the mode in which to open a database.

     SORT.OLDEST_FIRST
       Sort by message date, oldest first.

     SORT.NEWEST_FIRST
       Sort by message date, newest first.

     SORT.MESSAGE_ID
       Sort by email message ID.

     SORT.UNSORTED
       Do not apply a special sort order (returns results in document id     
       order). 

   .. automethod:: set_sort

   .. attribute::  sort

      Instance attribute :attr:`sort` contains the sort order (see
      :attr:`Query.SORT`) if explicitely specified via
      :meth:`set_sort`. By default it is set to `None`.

   .. automethod:: search_threads

   .. automethod:: search_messages

   .. automethod:: count_messages


:class:`Messages` -- A bunch of messages
----------------------------------------

.. autoclass:: Messages

   .. automethod:: collect_tags

   .. automethod:: __len__

:class:`Message` -- A single message
----------------------------------------

.. autoclass:: Message

   .. automethod:: get_message_id

   .. automethod:: get_thread_id

   .. automethod:: get_replies

   .. automethod:: get_filename

   .. attribute:: FLAG

        FLAG.MATCH 
          This flag is automatically set by a
	  Query.search_threads on those messages that match the
	  query. This allows us to distinguish matches from the rest
	  of the messages in that thread.

  .. automethod:: get_flag

   .. automethod:: set_flag
   
   .. automethod:: get_date

   .. automethod:: get_header

   .. automethod:: get_tags

   .. automethod:: remove_tag

   .. automethod:: add_tag

   .. automethod:: remove_all_tags

   .. automethod:: freeze

   .. automethod:: thaw

   .. automethod:: format_as_text

   .. automethod:: __str__


:class:`Tags` -- Notmuch tags
-----------------------------

.. autoclass:: Tags
   :members:

   .. automethod:: __len__

   .. automethod:: __str__


:class:`notmuch.Threads` -- Threads iterator
-----------------------------------------------------

.. autoclass:: notmuch.Threads

   .. automethod:: __len__

   .. automethod:: __str__

:class:`Thread` -- A single thread
------------------------------------

.. autoclass:: Thread

  .. automethod:: get_thread_id

  .. automethod:: get_total_messages

  .. automethod:: get_toplevel_messages

  .. automethod:: get_matched_messages

  .. automethod:: get_authors

  .. automethod:: get_subject

  .. automethod:: get_oldest_date

  .. automethod:: get_newest_date

  .. automethod:: get_tags

  .. automethod:: __str__


:class:`Filenames` -- An iterator over filenames
------------------------------------------------

.. autoclass:: notmuch.database.Filenames

   .. automethod:: notmuch.database.Filenames.__len__

:class:`notmuch.database.Directoy` -- A directory entry in the database
------------------------------------------------------------------------

.. autoclass:: notmuch.database.Directory

   .. automethod:: notmuch.database.Directory.get_child_files

   .. automethod:: notmuch.database.Directory.get_child_directories

   .. automethod:: notmuch.database.Directory.get_mtime

   .. automethod:: notmuch.database.Directory.set_mtime

   .. autoattribute:: notmuch.database.Directory.mtime

   .. autoattribute:: notmuch.database.Directory.path

:exc:`NotmuchError` -- A Notmuch execution error
------------------------------------------------
.. autoexception:: NotmuchError
   :members:

   This execption inherits directly from :exc:`Exception` and is raised on errors during the notmuch execution.

:class:`STATUS` -- Notmuch operation return status
--------------------------------------------------

.. data:: STATUS

  STATUS is a class, whose attributes provide constants that serve as return indicators for notmuch functions. Currently the following ones are defined. For possible return values and specific meaning for each method, see the method description.

  * SUCCESS
  * OUT_OF_MEMORY
  * READ_ONLY_DATABASE
  * XAPIAN_EXCEPTION
  * FILE_ERROR
  * FILE_NOT_EMAIL
  * DUPLICATE_MESSAGE_ID
  * NULL_POINTER
  * TAG_TOO_LONG
  * UNBALANCED_FREEZE_THAW
  * NOT_INITIALIZED


Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
