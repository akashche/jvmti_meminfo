<!--
 Copyright 2017, akashche at redhat.com.
 
 This code is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License version 2 only, as
 published by the Free Software Foundation.
 
 This code is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 version 2 for more details (a copy is included in the LICENSE.md file that
 accompanied this code).
-->
<!--
Template to filter agent valgrind (memcheck) output from JVM.

Run valgrind with XML output (replace dashes with double ones):

valgrind \
    -leak-check=yes \
    -show-reachable=yes \
    -track-origins=yes \
    -xml=yes \
    -xml-file=out.xml \
    java -agentpath:...

Filter output:

xsltproc ../resources/vgfilter.xsl out.xml > out.txt
-->
<xsl:stylesheet version="1.0" 
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml" omit-xml-declaration="yes" version="1.0" encoding="utf-8" indent="yes"/>
<xsl:strip-space elements="*"/>

<xsl:template match="/valgrindoutput">
    <xsl:apply-templates select="error/stack[frame/obj/text()[contains(.,'libmemlog_agent.so')]]"/>
 </xsl:template>

<xsl:template match="stack">
    <xsl:value-of select="../kind"/><xsl:text>: </xsl:text>
    <xsl:value-of select="../what"/><xsl:text>: </xsl:text>
    <xsl:value-of select="../xwhat/text"/><xsl:text>&#xa;</xsl:text>
    <xsl:for-each select="frame">
        <xsl:text>    </xsl:text>
        <xsl:value-of select="fn"/><xsl:text>:</xsl:text>
        <xsl:value-of select="file"/><xsl:text>:</xsl:text>
        <xsl:value-of select="line"/><xsl:text>&#xa;</xsl:text>
    </xsl:for-each>
    <xsl:text>&#xa;</xsl:text>
</xsl:template>

</xsl:stylesheet>
