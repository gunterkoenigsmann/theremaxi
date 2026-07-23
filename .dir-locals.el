;;; Indentation for this project: tabs, four columns wide.
;;;
;;; The perl code has been tab indented since 2017 and the C follows it, so
;;; nothing here changes how any existing file looks - it only stops Emacs from
;;; quietly inserting spaces into new lines.

((nil . ((indent-tabs-mode . t)
         (tab-width . 4)))

 (perl-mode . ((perl-indent-level . 4)))

 (cperl-mode . ((cperl-indent-level . 4)
                (cperl-tab-always-indent . t)
                (cperl-continued-statement-offset . 4)))

 (c-mode . ((c-basic-offset . 4)
            (c-file-style . "linux")))

 (sh-mode . ((sh-basic-offset . 4)))

 (cmake-mode . ((cmake-tab-width . 4)))

 ;; generated, and JSON has no tabs
 (json-mode . ((indent-tabs-mode . nil)
               (js-indent-level . 3))))
