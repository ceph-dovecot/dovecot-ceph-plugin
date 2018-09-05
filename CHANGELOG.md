# Change Log

## [0.0.13](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.13) (2018-09-05)

[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.12...0.0.13)

**Implemented enhancements:**

- rbox\_save\_update\_header\_flags before commiting save transaction [\#183](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/183)
- add changelog file [\#168](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/168)

**Fixed bugs:**

- Thread::try\_create\(\): pthread\_create failed with error 11 [\#188](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/188)
- doveadm backup \(rbox-\> mdbox\) stops if mailbox index has invalid entries [\#182](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/182)

**Merged pull requests:**

- Jrse cleanup [\#187](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/187) ([jrse](https://github.com/jrse))
- Jrse \#182 [\#186](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/186) ([jrse](https://github.com/jrse))
- Update issue templates [\#184](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/184) ([jrse](https://github.com/jrse))
- Jrse changelog [\#181](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/181) ([jrse](https://github.com/jrse))

## [0.0.12](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.12) (2018-07-18)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.11...0.0.12)

**Implemented enhancements:**

- doveadm rmb unit tests [\#174](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/174)
- doveadm rmb ls  shows orphaned objects [\#172](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/172)
- create delete all objects command for rmb CLI [\#171](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/171)
- Ls orphaned objects \#172 [\#177](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/177) ([jrse](https://github.com/jrse))
- Doveadm rmb unit tests \#174 [\#176](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/176) ([jrse](https://github.com/jrse))
- \#171: supports rmb -u \<user\> delete - --yes-i-really-really-mean-it [\#173](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/173) ([jrse](https://github.com/jrse))

**Fixed bugs:**

- doveadm rmb delete crashes if object doesn't exist [\#175](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/175)

**Closed issues:**

- cleanup log messages and disable entry-exit function log by default [\#178](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/178)

**Merged pull requests:**

- release version 0.0.12 [\#180](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/180) ([jrse](https://github.com/jrse))
- Jrse cleanup log \#178 [\#179](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/179) ([jrse](https://github.com/jrse))

## [0.0.11](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.11) (2018-07-10)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.10...0.0.11)

**Implemented enhancements:**

- Savelog: handle move operations differently [\#156](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/156)
- Restore Index \(update xattributes\) [\#155](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/155)
- doveadm force-resync -\> repair all user mailboxes at once [\#149](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/149)
- doveadm force-resync extension to clean up unreferenced objects [\#147](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/147)
- doveadm rbox check -u user [\#113](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/113)

**Fixed bugs:**

- Some more SCA fixes [\#162](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/162) ([dalgaaf](https://github.com/dalgaaf))

**Closed issues:**

- use imaptest copybox to eval. copy  [\#165](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/165)
- modify spec file to copy doveadm rmb plugin to doveadm plugin directory [\#164](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/164)
- doveadm rmb mailbox delete cmd [\#163](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/163)
- Delete UserAccount [\#108](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/108)

**Merged pull requests:**

- Jrse 2018 06 29 \#165 [\#170](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/170) ([jrse](https://github.com/jrse))
- 20180604 jrse\#156 [\#169](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/169) ([jrse](https://github.com/jrse))
- 20180604 jrse \#156 [\#167](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/167) ([jrse](https://github.com/jrse))
- Jrse 2018 06 29 \#165 [\#166](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/166) ([jrse](https://github.com/jrse))

## [0.0.10](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.10) (2018-06-04)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.9...0.0.10)

**Implemented enhancements:**

- create a logfile which contains all added mail objects [\#148](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/148)
- check for invalid mail objects in user namespace [\#142](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/142)

**Fixed bugs:**

- doveadm force resync \(repair\) without index file [\#150](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/150)
- imap process killed with signal 6 \(copy failure\) [\#143](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/143)
- fixes for g++ and clang++ compiler warnings [\#158](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/158) ([dalgaaf](https://github.com/dalgaaf))
- Various fixes from SCA [\#157](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/157) ([dalgaaf](https://github.com/dalgaaf))

**Closed issues:**

- Setup "real" ceph cluster [\#104](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/104)

**Merged pull requests:**

- removed regex, due to build issues on suse server [\#160](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/160) ([jrse](https://github.com/jrse))
- Fixes from SCA [\#154](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/154) ([dalgaaf](https://github.com/dalgaaf))
- Logfile \#148 [\#151](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/151) ([jrse](https://github.com/jrse))

## [0.0.9](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.9) (2018-05-23)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.8-cpy-rmb-fix...0.0.9)

**Merged pull requests:**

- Rmb fix [\#146](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/146) ([jrse](https://github.com/jrse))

## [0.0.8-cpy-rmb-fix](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.8-cpy-rmb-fix) (2018-05-23)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.8...0.0.8-cpy-rmb-fix)

**Merged pull requests:**

- Fix some smaller issues [\#141](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/141) ([dalgaaf](https://github.com/dalgaaf))

## [0.0.8](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.8) (2018-05-15)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.7...0.0.8)

**Closed issues:**

- Thread::try\_create\(\): pthread\_create failed with error 11 [\#139](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/139)
- pop3 travis test [\#138](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/138)
- Inbox.Inbox mailbox [\#134](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/134)

## [0.0.7](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.7) (2018-05-03)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.6...0.0.7)

## [0.0.6](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.6) (2018-04-20)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.5...0.0.6)

**Implemented enhancements:**

- RadosDictionary: atomic Increment / Decrement [\#132](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/132)

**Fixed bugs:**

- copy mail from raw\_storage \(lmtp\) [\#136](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/136)
- copy / move mail Error [\#135](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/135)

**Closed issues:**

- rmb tool \(unknown object in storage pool\) [\#137](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/137)
- Fork imap process [\#133](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/133)

## [0.0.5](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.5) (2018-04-16)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.4...0.0.5)

**Implemented enhancements:**

- travis ci, mount cephfs  [\#126](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/126)

## [0.0.4](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.4) (2018-04-03)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.3...0.0.4)

**Implemented enhancements:**

- travis CI support for imaptests [\#97](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/97)

**Fixed bugs:**

- doveadm force-resync \(rbox\) [\#130](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/130)
- zlib: error trailer has wrong crc [\#129](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/129)

**Closed issues:**

- Write all Immutable Mail metadata to single xattribute [\#124](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/124)
- rmb tool display flag names instead of hex value [\#120](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/120)
- Unable to find the jansson headers [\#114](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/114)
- Unit test update / remove Flags and keywords [\#105](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/105)
- Integrationtest: Use ceph fs for index files and cache [\#103](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/103)

## [0.0.3](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.3) (2018-02-22)
[Full Changelog](https://github.com/ceph-dovecot/dovecot-ceph-plugin/compare/0.0.2...0.0.3)

**Fixed bugs:**

- disabling all xattributes [\#123](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/123)
- zlib and dovecot master  [\#112](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/112)

**Closed issues:**

- ceph compression hint for objectoperation [\#122](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/122)
- backup mdbox -\> rbox \(receive.date\) [\#119](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/119)
- backup mdbox  -\> rbox [\#118](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/118)
- configuration: rbox\_pool\_name not used \(backup\) [\#117](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/117)
- Rados dictionary \(buffer assertion\) [\#116](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/116)
- testing with one mailbox [\#115](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/115)
- Rbox read mail \(buffer\) [\#102](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/102)

## [0.0.2](https://github.com/ceph-dovecot/dovecot-ceph-plugin/tree/0.0.2) (2017-12-11)
**Implemented enhancements:**

- save and update metadata configuration [\#98](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/98)
- librmb, review interfaces [\#93](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/93)
- add googletest: read\_mail [\#92](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/92)
- googletest for copy and move mail [\#80](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/80)
- read mail, unnecessary stat? [\#79](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/79)

**Fixed bugs:**

- copy mail : creating a mail duplicate [\#111](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/111)
- Quota reached -\> exception  [\#110](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/110)
- Dictionary and Quota plugin [\#100](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/100)

**Closed issues:**

- Ceph Namespace for user emails [\#109](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/109)
- log errors on error level with RADOS errno [\#107](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/107)
- imap crashes with signal 11 if rados connect fails [\#106](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/106)
- Rmbtool -p default pool [\#101](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/101)
- Integration test sync\_rbox\_2 [\#99](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/99)
- Travis CI support for integrationtests [\#96](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/96)
- Test for LDA [\#95](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/95)
- rmb tool - write manpage [\#94](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/94)
- librmb headers are not installed via 'make install' [\#88](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/88)
- make distcheck not working [\#86](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/86)
- make dist does not package all needed files to build [\#82](https://github.com/ceph-dovecot/dovecot-ceph-plugin/issues/82)

**Merged pull requests:**

- make distcheck [\#90](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/90) ([peter-mauritius](https://github.com/peter-mauritius))
- fix indentation and trailing whitespaces [\#89](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/89) ([dalgaaf](https://github.com/dalgaaf))
- update spec file with versio from OBS [\#87](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/87) ([dalgaaf](https://github.com/dalgaaf))
- make dist not complete [\#85](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/85) ([peter-mauritius](https://github.com/peter-mauritius))
- ignore some files from the tests subdirectory [\#84](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/84) ([dalgaaf](https://github.com/dalgaaf))
- add script to generate archive for package build [\#83](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/83) ([dalgaaf](https://github.com/dalgaaf))
- Travis support [\#81](https://github.com/ceph-dovecot/dovecot-ceph-plugin/pull/81) ([peter-mauritius](https://github.com/peter-mauritius))



\* *This Change Log was automatically generated by [github_changelog_generator](https://github.com/skywinder/Github-Changelog-Generator)*
