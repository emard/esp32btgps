<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.1" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:kml="http://www.opengis.net/kml/2.2">
  <xsl:output method="text" indent="yes" encoding="utf-8"/>
  
  <!-- usage
  xsltproc kml2csv.xsl file.kml > file.csv
  localc file.csv
  -->

  <!-- Removes all nodes with any empty text -->
  <xsl:template match="*[.='']"/>

  <!-- Removes all nodes with any empty attribute -->
  <xsl:template match="*[@*='']"/>

  <xsl:template match="text()"/>

  <xsl:template match="/">
    <xsl:text>"travel [m]","IRI100 [mm/m]","head [°]","lon [°]","lat [°]","time"&#xA;</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="kml:Document">
    <xsl:for-each select="kml:Folder">
      <xsl:for-each select="kml:Placemark[kml:Style/kml:IconStyle]">
        <xsl:value-of select="position()"/><xsl:text>00,</xsl:text>
        <xsl:value-of select="kml:name"/><xsl:text>,</xsl:text>
        <xsl:value-of select="kml:Style/kml:IconStyle/kml:heading"/><xsl:text>,</xsl:text>
        <xsl:value-of select="kml:Point/kml:coordinates"/><xsl:text>,</xsl:text>
        <xsl:value-of select="kml:TimeStamp/kml:when"/><xsl:text>,</xsl:text>
        <xsl:call-template name="tokenize">
          <xsl:with-param name="text" select="kml:description"/>
        </xsl:call-template>
        <xsl:text>&#xA;</xsl:text>
      </xsl:for-each>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="text/text()" name="tokenize">
    <xsl:param name="text" select="."/>
    <xsl:param name="separator" select="'&#xA;'"/>
    <xsl:choose>
      <xsl:when test="not(contains($text, $separator))">
        <xsl:text>"</xsl:text><xsl:value-of select="normalize-space($text)"/><xsl:text>",</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>"</xsl:text><xsl:value-of select="normalize-space(substring-before($text, $separator))"/><xsl:text>",</xsl:text>
        <xsl:call-template name="tokenize">
          <xsl:with-param name="text" select="substring-after($text, $separator)"/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

</xsl:stylesheet>
