;; test-lib.el --- auxiliary stuff for Notmuch Emacs tests.
;;
;; Copyright © Carl Worth
;; Copyright © David Edmondson
;;
;; This file is part of Notmuch test suit.
;;
;; Notmuch is free software: you can redistribute it and/or modify it
;; under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.
;;
;; Notmuch is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with Notmuch.  If not, see <http://www.gnu.org/licenses/>.
;;
;; Authors: Dmitry Kurochkin <dmitry.kurochkin@gmail.com>

(require 'cl)	;; This code is generally used uncompiled.

;; `read-file-name' by default uses `completing-read' function to read
;; user input.  It does not respect `standard-input' variable which we
;; use in tests to provide user input.  So replace it with a plain
;; `read' call.
(setq read-file-name-function (lambda (&rest _) (read)))

;; Work around a bug in emacs 23.1 and emacs 23.2 which prevents
;; noninteractive (kill-emacs) from emacsclient.
(if (and (= emacs-major-version 23) (< emacs-minor-version 3))
  (defadvice kill-emacs (before disable-yes-or-no-p activate)
    "Disable yes-or-no-p before executing kill-emacs"
    (defun yes-or-no-p (prompt) t)))

(defun notmuch-test-wait ()
  "Wait for process completion."
  (while (get-buffer-process (current-buffer))
    (sleep-for 0.1)))

(defun test-output (&optional filename)
  "Save current buffer to file FILENAME.  Default FILENAME is OUTPUT."
  (write-region (point-min) (point-max) (or filename "OUTPUT")))

(defun test-visible-output (&optional filename)
  "Save visible text in current buffer to file FILENAME.  Default
FILENAME is OUTPUT."
  (let ((text (visible-buffer-string)))
    (with-temp-file (or filename "OUTPUT") (insert text))))

(defun visible-buffer-string ()
  "Same as `buffer-string', but excludes invisible text and
removes any text properties."
  (visible-buffer-substring (point-min) (point-max)))

(defun visible-buffer-substring (start end)
  "Same as `buffer-substring-no-properties', but excludes
invisible text."
  (let (str)
    (while (< start end)
      (let ((next-pos (next-char-property-change start end)))
	(when (not (invisible-p start))
	  (setq str (concat str (buffer-substring-no-properties
				 start next-pos))))
	(setq start next-pos)))
    str))

(defun orphan-watchdog (pid)
  "Periodically check that the process with id PID is still
running, quit if it terminated."
  (if (not (process-attributes pid))
      (kill-emacs)
    (run-at-time "1 min" nil 'orphan-watchdog pid)))

(defun hook-counter (hook)
  "Count how many times a hook is called.  Increments
`hook'-counter variable value if it is bound, otherwise does
nothing."
  (let ((counter (intern (concat (symbol-name hook) "-counter"))))
    (if (boundp counter)
	(set counter (1+ (symbol-value counter))))))

(defun add-hook-counter (hook)
  "Add hook to count how many times `hook' is called."
  (add-hook hook (apply-partially 'hook-counter hook)))

(add-hook-counter 'notmuch-hello-mode-hook)
(add-hook-counter 'notmuch-hello-refresh-hook)

(defmacro notmuch-test-run (&rest body)
  "Evaluate a BODY of test expressions and output the result."
  `(with-temp-buffer
     (let ((buffer (current-buffer))
	   (result (progn ,@body)))
       (switch-to-buffer buffer)
       (insert (if (stringp result)
		   result
		 (prin1-to-string result)))
       (test-output))))

(defun notmuch-test-report-unexpected (output expected)
  "Report that the OUTPUT does not match the EXPECTED result."
  (concat "Expect:\t" (prin1-to-string expected) "\n"
	  "Output:\t" (prin1-to-string output) "\n"))

(defun notmuch-test-expect-equal (output expected)
  "Compare OUTPUT with EXPECTED. Report any discrepencies."
  (if (equal output expected)
      t
    (cond
     ((and (listp output)
	   (listp expected))
      ;; Reporting the difference between two lists is done by
      ;; reporting differing elements of OUTPUT and EXPECTED
      ;; pairwise. This is expected to make analysis of failures
      ;; simpler.
      (apply #'concat (loop for o in output
			    for e in expected
			    if (not (equal o e))
			    collect (notmuch-test-report-unexpected o e))))

     (t
      (notmuch-test-report-unexpected output expected)))))
