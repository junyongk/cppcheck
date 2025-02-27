/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2019 Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QObject>
#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QFile>
#include <QDir>
#include "projectfile.h"
#include "common.h"

#include "path.h"

static const char ProjectElementName[] = "project";
static const char ProjectVersionAttrib[] = "version";
static const char ProjectFileVersion[] = "1";
static const char BuildDirElementName[] = "builddir";
static const char ImportProjectElementName[] = "importproject";
static const char AnalyzeAllVsConfigsElementName[] = "analyze-all-vs-configs";
static const char IncludeDirElementName[] = "includedir";
static const char DirElementName[] = "dir";
static const char DirNameAttrib[] = "name";
static const char DefinesElementName[] = "defines";
static const char DefineName[] = "define";
static const char DefineNameAttrib[] = "name";
static const char UndefinesElementName[] = "undefines";
static const char UndefineName[] = "undefine";
static const char PathsElementName[] = "paths";
static const char PathName[] = "dir";
static const char PathNameAttrib[] = "name";
static const char RootPathName[] = "root";
static const char RootPathNameAttrib[] = "name";
static const char IgnoreElementName[] = "ignore";
static const char IgnorePathName[] = "path";
static const char IgnorePathNameAttrib[] = "name";
static const char ExcludeElementName[] = "exclude";
static const char ExcludePathName[] = "path";
static const char ExcludePathNameAttrib[] = "name";
static const char LibrariesElementName[] = "libraries";
static const char LibraryElementName[] = "library";
static const char PlatformElementName[] = "platform";
static const char SuppressionsElementName[] = "suppressions";
static const char SuppressionElementName[] = "suppression";
static const char AddonElementName[] = "addon";
static const char AddonsElementName[] = "addons";
static const char ToolElementName[] = "tool";
static const char ToolsElementName[] = "tools";
static const char TagsElementName[] = "tags";
static const char TagElementName[] = "tag";
static const char CheckHeadersElementName[] = "check-headers";
static const char CheckUnusedTemplatesElementName[] = "check-unused-templates";
static const char MaxCtuDepthElementName[] = "max-ctu-depth";
static const char CheckUnknownFunctionReturn[] = "check-unknown-function-return-values";
static const char CheckAllFunctionParameterValues[] = "check-all-function-parameter-values";
static const char Name[] = "name";

ProjectFile::ProjectFile(QObject *parent) :
    QObject(parent)
{
    clear();
}

ProjectFile::ProjectFile(const QString &filename, QObject *parent) :
    QObject(parent),
    mFilename(filename)
{
    clear();
    read();
}

void ProjectFile::clear()
{
    mRootPath.clear();
    mBuildDir.clear();
    mImportProject.clear();
    mAnalyzeAllVsConfigs = true;
    mIncludeDirs.clear();
    mDefines.clear();
    mUndefines.clear();
    mPaths.clear();
    mExcludedPaths.clear();
    mLibraries.clear();
    mPlatform.clear();
    mSuppressions.clear();
    mAddons.clear();
    mClangAnalyzer = mClangTidy = false;
    mAnalyzeAllVsConfigs = false;
    mCheckHeaders = true;
    mCheckUnusedTemplates = false;
    mMaxCtuDepth = 10;
    mCheckAllFunctionParameterValues = false;
    mCheckUnknownFunctionReturn.clear();
}

bool ProjectFile::read(const QString &filename)
{
    if (!filename.isEmpty())
        mFilename = filename;

    QFile file(mFilename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    clear();

    QXmlStreamReader xmlReader(&file);
    bool insideProject = false;
    bool projectTagFound = false;
    while (!xmlReader.atEnd()) {
        switch (xmlReader.readNext()) {
        case QXmlStreamReader::StartElement:
            if (xmlReader.name() == ProjectElementName) {
                insideProject = true;
                projectTagFound = true;
            }
            // Read root path from inside project element
            if (insideProject && xmlReader.name() == RootPathName)
                readRootPath(xmlReader);

            // Read root path from inside project element
            if (insideProject && xmlReader.name() == BuildDirElementName)
                readBuildDir(xmlReader);

            // Find paths to check from inside project element
            if (insideProject && xmlReader.name() == PathsElementName)
                readCheckPaths(xmlReader);

            if (insideProject && xmlReader.name() == ImportProjectElementName)
                readImportProject(xmlReader);

            if (insideProject && xmlReader.name() == AnalyzeAllVsConfigsElementName)
                mAnalyzeAllVsConfigs = readBool(xmlReader);

            if (insideProject && xmlReader.name() == CheckHeadersElementName)
                mCheckHeaders = readBool(xmlReader);

            if (insideProject && xmlReader.name() == CheckUnusedTemplatesElementName)
                mCheckUnusedTemplates = readBool(xmlReader);

            // Find include directory from inside project element
            if (insideProject && xmlReader.name() == IncludeDirElementName)
                readIncludeDirs(xmlReader);

            // Find preprocessor define from inside project element
            if (insideProject && xmlReader.name() == DefinesElementName)
                readDefines(xmlReader);

            // Find preprocessor define from inside project element
            if (insideProject && xmlReader.name() == UndefinesElementName)
                readStringList(mUndefines, xmlReader, UndefineName);

            // Find exclude list from inside project element
            if (insideProject && xmlReader.name() == ExcludeElementName)
                readExcludes(xmlReader);

            // Find ignore list from inside project element
            // These are read for compatibility
            if (insideProject && xmlReader.name() == IgnoreElementName)
                readExcludes(xmlReader);

            // Find libraries list from inside project element
            if (insideProject && xmlReader.name() == LibrariesElementName)
                readStringList(mLibraries, xmlReader,LibraryElementName);

            if (insideProject && xmlReader.name() == PlatformElementName)
                readPlatform(xmlReader);

            // Find suppressions list from inside project element
            if (insideProject && xmlReader.name() == SuppressionsElementName)
                readSuppressions(xmlReader);

            // Unknown function return values
            if (insideProject && xmlReader.name() == CheckUnknownFunctionReturn)
                readStringList(mCheckUnknownFunctionReturn, xmlReader, Name);

            // check all function parameter values
            if (insideProject && xmlReader.name() == CheckAllFunctionParameterValues)
                mCheckAllFunctionParameterValues = true;

            // Addons
            if (insideProject && xmlReader.name() == AddonsElementName)
                readStringList(mAddons, xmlReader, AddonElementName);

            // Tools
            if (insideProject && xmlReader.name() == ToolsElementName) {
                QStringList tools;
                readStringList(tools, xmlReader, ToolElementName);
                mClangAnalyzer = tools.contains(CLANG_ANALYZER);
                mClangTidy = tools.contains(CLANG_TIDY);
            }

            if (insideProject && xmlReader.name() == TagsElementName)
                readStringList(mTags, xmlReader, TagElementName);

            if (insideProject && xmlReader.name() == MaxCtuDepthElementName)
                mMaxCtuDepth = readInt(xmlReader, mMaxCtuDepth);

            break;

        case QXmlStreamReader::EndElement:
            if (xmlReader.name() == ProjectElementName)
                insideProject = false;
            break;

        // Not handled
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    }

    file.close();
    return projectTagFound;
}

void ProjectFile::readRootPath(QXmlStreamReader &reader)
{
    QXmlStreamAttributes attribs = reader.attributes();
    QString name = attribs.value(QString(), RootPathNameAttrib).toString();
    if (!name.isEmpty())
        mRootPath = name;
}

void ProjectFile::readBuildDir(QXmlStreamReader &reader)
{
    mBuildDir.clear();
    do {
        const QXmlStreamReader::TokenType type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::Characters:
            mBuildDir = reader.text().toString();
        case QXmlStreamReader::EndElement:
            return;
        // Not handled
        case QXmlStreamReader::StartElement:
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (1);
}

void ProjectFile::readImportProject(QXmlStreamReader &reader)
{
    mImportProject.clear();
    do {
        const QXmlStreamReader::TokenType type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::Characters:
            mImportProject = reader.text().toString();
        case QXmlStreamReader::EndElement:
            return;
        // Not handled
        case QXmlStreamReader::StartElement:
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (1);
}

bool ProjectFile::readBool(QXmlStreamReader &reader)
{
    bool ret = false;
    do {
        const QXmlStreamReader::TokenType type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::Characters:
            ret = (reader.text().toString() == "true");
        case QXmlStreamReader::EndElement:
            return ret;
        // Not handled
        case QXmlStreamReader::StartElement:
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (1);
}

int ProjectFile::readInt(QXmlStreamReader &reader, int defaultValue)
{
    int ret = defaultValue;
    do {
        const QXmlStreamReader::TokenType type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::Characters:
            ret = reader.text().toString().toInt();
        case QXmlStreamReader::EndElement:
            return ret;
        // Not handled
        case QXmlStreamReader::StartElement:
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (1);
}

void ProjectFile::readIncludeDirs(QXmlStreamReader &reader)
{
    QXmlStreamReader::TokenType type;
    bool allRead = false;
    do {
        type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::StartElement:

            // Read dir-elements
            if (reader.name().toString() == DirElementName) {
                QXmlStreamAttributes attribs = reader.attributes();
                QString name = attribs.value(QString(), DirNameAttrib).toString();
                if (!name.isEmpty())
                    mIncludeDirs << name;
            }
            break;

        case QXmlStreamReader::EndElement:
            if (reader.name().toString() == IncludeDirElementName)
                allRead = true;
            break;

        // Not handled
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (!allRead);
}

void ProjectFile::readDefines(QXmlStreamReader &reader)
{
    QXmlStreamReader::TokenType type;
    bool allRead = false;
    do {
        type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::StartElement:
            // Read define-elements
            if (reader.name().toString() == DefineName) {
                QXmlStreamAttributes attribs = reader.attributes();
                QString name = attribs.value(QString(), DefineNameAttrib).toString();
                if (!name.isEmpty())
                    mDefines << name;
            }
            break;

        case QXmlStreamReader::EndElement:
            if (reader.name().toString() == DefinesElementName)
                allRead = true;
            break;

        // Not handled
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (!allRead);
}

void ProjectFile::readCheckPaths(QXmlStreamReader &reader)
{
    QXmlStreamReader::TokenType type;
    bool allRead = false;
    do {
        type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::StartElement:

            // Read dir-elements
            if (reader.name().toString() == PathName) {
                QXmlStreamAttributes attribs = reader.attributes();
                QString name = attribs.value(QString(), PathNameAttrib).toString();
                if (!name.isEmpty())
                    mPaths << name;
            }
            break;

        case QXmlStreamReader::EndElement:
            if (reader.name().toString() == PathsElementName)
                allRead = true;
            break;

        // Not handled
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (!allRead);
}

void ProjectFile::readExcludes(QXmlStreamReader &reader)
{
    QXmlStreamReader::TokenType type;
    bool allRead = false;
    do {
        type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::StartElement:
            // Read exclude-elements
            if (reader.name().toString() == ExcludePathName) {
                QXmlStreamAttributes attribs = reader.attributes();
                QString name = attribs.value(QString(), ExcludePathNameAttrib).toString();
                if (!name.isEmpty())
                    mExcludedPaths << name;
            }
            // Read ignore-elements - deprecated but support reading them
            else if (reader.name().toString() == IgnorePathName) {
                QXmlStreamAttributes attribs = reader.attributes();
                QString name = attribs.value(QString(), IgnorePathNameAttrib).toString();
                if (!name.isEmpty())
                    mExcludedPaths << name;
            }
            break;

        case QXmlStreamReader::EndElement:
            if (reader.name().toString() == IgnoreElementName)
                allRead = true;
            if (reader.name().toString() == ExcludeElementName)
                allRead = true;
            break;

        // Not handled
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (!allRead);
}

void ProjectFile::readPlatform(QXmlStreamReader &reader)
{
    do {
        const QXmlStreamReader::TokenType type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::Characters:
            mPlatform = reader.text().toString();
        case QXmlStreamReader::EndElement:
            return;
        // Not handled
        case QXmlStreamReader::StartElement:
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (1);
}


void ProjectFile::readSuppressions(QXmlStreamReader &reader)
{
    QXmlStreamReader::TokenType type;
    do {
        type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::StartElement:
            // Read library-elements
            if (reader.name().toString() == SuppressionElementName) {
                Suppressions::Suppression suppression;
                if (reader.attributes().hasAttribute(QString(),"fileName"))
                    suppression.fileName = reader.attributes().value(QString(),"fileName").toString().toStdString();
                if (reader.attributes().hasAttribute(QString(),"lineNumber"))
                    suppression.lineNumber = reader.attributes().value(QString(),"lineNumber").toInt();
                if (reader.attributes().hasAttribute(QString(),"symbolName"))
                    suppression.symbolName = reader.attributes().value(QString(),"symbolName").toString().toStdString();
                type = reader.readNext();
                if (type == QXmlStreamReader::Characters) {
                    suppression.errorId = reader.text().toString().toStdString();
                }
                mSuppressions << suppression;
            }
            break;

        case QXmlStreamReader::EndElement:
            if (reader.name().toString() != SuppressionElementName)
                return;
            break;

        // Not handled
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (true);
}


void ProjectFile::readStringList(QStringList &stringlist, QXmlStreamReader &reader, const char elementname[])
{
    QXmlStreamReader::TokenType type;
    bool allRead = false;
    do {
        type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::StartElement:
            // Read library-elements
            if (reader.name().toString() == elementname) {
                type = reader.readNext();
                if (type == QXmlStreamReader::Characters) {
                    QString text = reader.text().toString();
                    stringlist << text;
                }
            }
            break;

        case QXmlStreamReader::EndElement:
            if (reader.name().toString() != elementname)
                allRead = true;
            break;

        // Not handled
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            break;
        }
    } while (!allRead);
}

QList<Suppressions::Suppression> ProjectFile::getCheckSuppressions() const
{
    QList<Suppressions::Suppression> ret;
    const std::string projectFilePath = Path::getPathFromFilename(mFilename.toStdString());
    foreach (Suppressions::Suppression suppression, getSuppressions()) {
        if (!suppression.fileName.empty() && suppression.fileName[0]!='*' && !Path::isAbsolute(suppression.fileName))
            suppression.fileName = Path::simplifyPath(projectFilePath) + suppression.fileName;

        ret.append(suppression);
    }
    return ret;
}

void ProjectFile::setIncludes(const QStringList &includes)
{
    mIncludeDirs = includes;
}

void ProjectFile::setDefines(const QStringList &defines)
{
    mDefines = defines;
}

void ProjectFile::setUndefines(const QStringList &undefines)
{
    mUndefines = undefines;
}

void ProjectFile::setCheckPaths(const QStringList &paths)
{
    mPaths = paths;
}

void ProjectFile::setExcludedPaths(const QStringList &paths)
{
    mExcludedPaths = paths;
}

void ProjectFile::setLibraries(const QStringList &libraries)
{
    mLibraries = libraries;
}

void ProjectFile::setPlatform(const QString &platform)
{
    mPlatform = platform;
}

void ProjectFile::setSuppressions(const QList<Suppressions::Suppression> &suppressions)
{
    mSuppressions = suppressions;
}

void ProjectFile::setAddons(const QStringList &addons)
{
    mAddons = addons;
}

bool ProjectFile::write(const QString &filename)
{
    if (!filename.isEmpty())
        mFilename = filename;

    QFile file(mFilename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument("1.0");
    xmlWriter.writeStartElement(ProjectElementName);
    xmlWriter.writeAttribute(ProjectVersionAttrib, ProjectFileVersion);

    if (!mRootPath.isEmpty()) {
        xmlWriter.writeStartElement(RootPathName);
        xmlWriter.writeAttribute(RootPathNameAttrib, mRootPath);
        xmlWriter.writeEndElement();
    }

    if (!mBuildDir.isEmpty()) {
        xmlWriter.writeStartElement(BuildDirElementName);
        xmlWriter.writeCharacters(mBuildDir);
        xmlWriter.writeEndElement();
    }

    if (!mPlatform.isEmpty()) {
        xmlWriter.writeStartElement(PlatformElementName);
        xmlWriter.writeCharacters(mPlatform);
        xmlWriter.writeEndElement();
    }

    if (!mImportProject.isEmpty()) {
        xmlWriter.writeStartElement(ImportProjectElementName);
        xmlWriter.writeCharacters(mImportProject);
        xmlWriter.writeEndElement();
    }

    xmlWriter.writeStartElement(AnalyzeAllVsConfigsElementName);
    xmlWriter.writeCharacters(mAnalyzeAllVsConfigs ? "true" : "false");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement(CheckHeadersElementName);
    xmlWriter.writeCharacters(mCheckHeaders ? "true" : "false");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement(CheckUnusedTemplatesElementName);
    xmlWriter.writeCharacters(mCheckUnusedTemplates ? "true" : "false");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement(MaxCtuDepthElementName);
    xmlWriter.writeCharacters(QString::number(mMaxCtuDepth));
    xmlWriter.writeEndElement();

    if (!mIncludeDirs.isEmpty()) {
        xmlWriter.writeStartElement(IncludeDirElementName);
        foreach (QString incdir, mIncludeDirs) {
            xmlWriter.writeStartElement(DirElementName);
            xmlWriter.writeAttribute(DirNameAttrib, incdir);
            xmlWriter.writeEndElement();
        }
        xmlWriter.writeEndElement();
    }

    if (!mDefines.isEmpty()) {
        xmlWriter.writeStartElement(DefinesElementName);
        foreach (QString define, mDefines) {
            xmlWriter.writeStartElement(DefineName);
            xmlWriter.writeAttribute(DefineNameAttrib, define);
            xmlWriter.writeEndElement();
        }
        xmlWriter.writeEndElement();
    }

    writeStringList(xmlWriter,
                    mUndefines,
                    UndefinesElementName,
                    UndefineName);

    if (!mPaths.isEmpty()) {
        xmlWriter.writeStartElement(PathsElementName);
        foreach (QString path, mPaths) {
            xmlWriter.writeStartElement(PathName);
            xmlWriter.writeAttribute(PathNameAttrib, path);
            xmlWriter.writeEndElement();
        }
        xmlWriter.writeEndElement();
    }

    if (!mExcludedPaths.isEmpty()) {
        xmlWriter.writeStartElement(ExcludeElementName);
        foreach (QString path, mExcludedPaths) {
            xmlWriter.writeStartElement(ExcludePathName);
            xmlWriter.writeAttribute(ExcludePathNameAttrib, path);
            xmlWriter.writeEndElement();
        }
        xmlWriter.writeEndElement();
    }

    writeStringList(xmlWriter,
                    mLibraries,
                    LibrariesElementName,
                    LibraryElementName);

    if (!mSuppressions.isEmpty()) {
        xmlWriter.writeStartElement(SuppressionsElementName);
        foreach (const Suppressions::Suppression &suppression, mSuppressions) {
            xmlWriter.writeStartElement(SuppressionElementName);
            if (!suppression.fileName.empty())
                xmlWriter.writeAttribute("fileName", QString::fromStdString(suppression.fileName));
            if (suppression.lineNumber > 0)
                xmlWriter.writeAttribute("lineNumber", QString::number(suppression.lineNumber));
            if (!suppression.symbolName.empty())
                xmlWriter.writeAttribute("symbolName", QString::fromStdString(suppression.symbolName));
            if (!suppression.errorId.empty())
                xmlWriter.writeCharacters(QString::fromStdString(suppression.errorId));
            xmlWriter.writeEndElement();
        }
        xmlWriter.writeEndElement();
    }

    writeStringList(xmlWriter,
                    mCheckUnknownFunctionReturn,
                    CheckUnknownFunctionReturn,
                    Name);

    if (mCheckAllFunctionParameterValues) {
        xmlWriter.writeStartElement(CheckAllFunctionParameterValues);
        xmlWriter.writeEndElement();
    }

    writeStringList(xmlWriter,
                    mAddons,
                    AddonsElementName,
                    AddonElementName);

    QStringList tools;
    if (mClangAnalyzer)
        tools << CLANG_ANALYZER;
    if (mClangTidy)
        tools << CLANG_TIDY;
    writeStringList(xmlWriter,
                    tools,
                    ToolsElementName,
                    ToolElementName);

    writeStringList(xmlWriter, mTags, TagsElementName, TagElementName);

    xmlWriter.writeEndDocument();
    file.close();
    return true;
}

void ProjectFile::writeStringList(QXmlStreamWriter &xmlWriter, const QStringList &stringlist, const char startelementname[], const char stringelementname[])
{
    if (stringlist.isEmpty())
        return;

    xmlWriter.writeStartElement(startelementname);
    foreach (QString str, stringlist) {
        xmlWriter.writeStartElement(stringelementname);
        xmlWriter.writeCharacters(str);
        xmlWriter.writeEndElement();
    }
    xmlWriter.writeEndElement();
}

QStringList ProjectFile::fromNativeSeparators(const QStringList &paths)
{
    QStringList ret;
    foreach (const QString &path, paths)
        ret << QDir::fromNativeSeparators(path);
    return ret;
}

QStringList ProjectFile::getAddonsAndTools() const
{
    QStringList ret(mAddons);
    if (mClangAnalyzer)
        ret << CLANG_ANALYZER;
    if (mClangTidy)
        ret << CLANG_TIDY;
    return ret;
}
