// Copyright 2019 Andrew Karasyov
//
// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cloudstorageapi/cloud_storage_client.h"
#include "csa_example_helper.h"
#include <fstream>
#include <functional>
#include <string>
#include <thread>

using namespace csa::example;

namespace {

std::string commandUsage;

void PrintUsage(int argc, char* argv[], std::string const& msg)
{
    std::string const cmd = argv[0];
    auto lastSlash = cmd.find_last_of('/');
    if (lastSlash == std::string::npos)
        lastSlash = cmd.find_last_of('\\');
    auto program = cmd.substr(lastSlash + 1);
    std::cerr << msg << "\nUsage: " << program << " <provider> <command> [arguments]\n\n"
              << "Commands:\n"
              << commandUsage << std::endl;
}

void Delete(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 1)
        throw NeedUsage("delete <object-id>");

    auto objectId = argv.at(0);
    auto status = client->Delete(objectId);
    if (status.Ok())
        std::cout << "Successfully deleted object " << objectId << std::endl;
    else
        std::cout << "Faild to delete object " << objectId << status << std::endl;
}

void ListFolder(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 1)
        throw NeedUsage("list-folder <folder-id>");

    auto folderName = argv.at(0);
    for (auto&& metadata : client->ListFolder(folderName))
    {
        if (!metadata)
            throw std::runtime_error(metadata.GetStatus().Message());

        std::visit(
            [](auto&& item) { std::cout << item.GetName() << std::endl
                                        << "\t" << item << std::endl
                                        << std::endl; },
            *metadata);
    }
}

void ListFolderWithPageSize(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 2)
        throw NeedUsage("list-folder-with-page-size <folder-path> <page-size>");

    auto folderName = argv.at(0);
    int pageSize = std::stoi(argv.at(1));
    for (auto&& metadata : client->ListFolder(folderName, csa::MaxResults(pageSize)))
    {
        if (!metadata)
            throw std::runtime_error(metadata.GetStatus().Message());

        std::visit(
            [](auto&& item) { std::cout << item.GetName() << std::endl
                                        << "\t" << item << std::endl
                                        << std::endl; },
            *metadata);
    }
}

void GetFolderMetadata(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 1)
        throw NeedUsage("get-folder-metadata <folder-path>");

    auto folderId = argv.at(0);
    csa::StatusOrVal<csa::FolderMetadata> folderMetadata = client->GetFolderMetadata(folderId);
    if (!folderMetadata)
    {
        throw std::runtime_error(folderMetadata.GetStatus().Message());
    }

    std::cout << "\'" << folderMetadata->GetName() << "\' " << *folderMetadata << std::endl;
}

void RenameFolder(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || (argv.size() != 2 && argv.size() != 4))
        throw NeedUsage("rename-folder <folder-id> <new-name> [<parent-id> <new-parent-id>]");

    auto folderId = argv.at(0);
    auto newName = argv.at(1);
    std::string parentId;
    std::string newParentid;
    if (argv.size() == 4)
    {
        parentId = argv.at(2);
        newParentid = argv.at(3);
    }
    csa::StatusOrVal<csa::FolderMetadata> folderMetadata =
        client->RenameFolder(folderId, newName, parentId, newParentid);
    if (!folderMetadata)
    {
        throw std::runtime_error(folderMetadata.GetStatus().Message());
    }

    std::cout << "Rename folder succeeded: id=\'" << folderId << "\' " << *folderMetadata << std::endl;
}

void PatchDeleteFolderMetadata(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 2)
        throw NeedUsage("patch-delete-folder-metadata <folder-id> <key>");

    auto folderId = argv.at(0);
    std::string key = argv.at(1);

    csa::StatusOrVal<csa::FolderMetadata> folderMeta = client->GetFolderMetadata(folderId);
    if (!folderMeta)
    {
        throw std::runtime_error(folderMeta.GetStatus().Message());
    }

    csa::FolderMetadata updateFolderMeta = *folderMeta;
    if (key == "modifiedTime")
    {
        auto epochStart = std::chrono::time_point<std::chrono::system_clock>{};
        updateFolderMeta.SetModifyTime(epochStart);
    }
    else if (key == "name")
        updateFolderMeta.SetName("");

    auto newUpdateFolderMeta = client->PatchFolderMetadata(folderId, folderMeta.Value(), updateFolderMeta);
    if (!newUpdateFolderMeta)
    {
        throw std::runtime_error(newUpdateFolderMeta.GetStatus().Message());
    }

    std::cout << "The folder \"" << folderMeta->GetName() << "\" updated. Updated metadata: " << *newUpdateFolderMeta
              << std::endl;
}

void GetFileMetadata(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 1)
        throw NeedUsage("get-file-metadata <file-path>");

    auto fileName = argv.at(0);
    csa::StatusOrVal<csa::FileMetadata> fileMetadata = client->GetFileMetadata(fileName);
    if (!fileMetadata)
    {
        throw std::runtime_error(fileMetadata.GetStatus().Message());
    }

    std::cout << "\'" << fileMetadata->GetName() << "\' " << *fileMetadata << std::endl;
}

void PatchDeleteFileMetadata(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 2)
        throw NeedUsage("patch-delete-file-metadata <file-id> <key>");

    auto fileId = argv.at(0);
    std::string key = argv.at(1);

    csa::StatusOrVal<csa::FileMetadata> fileMeta = client->GetFileMetadata(fileId);
    if (!fileMeta)
    {
        throw std::runtime_error(fileMeta.GetStatus().Message());
    }

    csa::FileMetadata updateFileMeta = *fileMeta;
    if (key == "mimeType")
        updateFileMeta.SetMimeTypeOpt(std::nullopt);
    else if (key == "modifiedTime")
    {
        auto epochStart = std::chrono::time_point<std::chrono::system_clock>{};
        updateFileMeta.SetModifyTime(epochStart);
    }
    else if (key == "name")
        updateFileMeta.SetName("");

    auto newUpdateFileMeta = client->PatchFileMetadata(fileId, fileMeta.Value(), updateFileMeta);
    if (!newUpdateFileMeta)
    {
        throw std::runtime_error(newUpdateFileMeta.GetStatus().Message());
    }

    std::cout << "The file \"" << fileMeta->GetName() << "\" updated. Updated metadata: " << *newUpdateFileMeta
              << std::endl;
}

void RenameFile(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || (argv.size() != 2 && argv.size() != 4))
        throw NeedUsage("rename-file <file-id> <new-name> [<parent-id> <new-parent-id>]");

    auto fileId = argv.at(0);
    auto newName = argv.at(1);
    std::string parentId;
    std::string newParentid;
    if (argv.size() == 4)
    {
        parentId = argv.at(2);
        newParentid = argv.at(3);
    }
    csa::StatusOrVal<csa::FileMetadata> fileMetadata = client->RenameFile(fileId, newName, parentId, newParentid);
    if (!fileMetadata)
    {
        throw std::runtime_error(fileMetadata.GetStatus().Message());
    }

    std::cout << "Rename file succeeded: id=\'" << fileId << "\' " << *fileMetadata << std::endl;
}

void InsertFile(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 3)
        throw NeedUsage("insert-file <parent-folder-id> <file-name> <file-content (string)>");

    auto folderId = argv.at(0);
    auto fileName = argv.at(1);
    auto contents = argv.at(2);

    // It runs multipart insertion if name is not empty.
    auto fileMetadata = client->InsertFile(folderId, fileName, contents);
    if (!fileMetadata)
        throw std::runtime_error(fileMetadata.GetStatus().Message());

    std::cout << "Insert file succeded: id=\'" << fileMetadata->GetCloudId() << "\' " << *fileMetadata << std::endl;
}

void UploadFile(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 3)
        throw NeedUsage("upload-file <src-file-name> <parent-folder-id> <file-name>");

    auto srcFileName = argv.at(0);
    auto folderId = argv.at(1);
    auto fileName = argv.at(2);

    auto fileMetadata = client->UploadFile(srcFileName, folderId, fileName);
    if (!fileMetadata)
        throw std::runtime_error(fileMetadata.GetStatus().Message());

    std::cout << "Uploaded file " << srcFileName << " succeded to cloud file: id=\'" << fileMetadata->GetCloudId()
              << "\' " << *fileMetadata << std::endl;
}

void UploadFileResumable(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 3)
        throw NeedUsage("upload-file-resumable <src-file-name> <parent-folder-id> <file-name>");

    auto srcFileName = argv.at(0);
    auto folderId = argv.at(1);
    auto fileName = argv.at(2);

    auto fileMetadata = client->UploadFile(srcFileName, folderId, fileName, csa::NewResumableUploadSession());
    if (!fileMetadata)
        throw std::runtime_error(fileMetadata.GetStatus().Message());

    std::cout << "Uploaded file " << srcFileName << " succeded to cloud file: id=\'" << fileMetadata->GetCloudId()
              << "\' " << *fileMetadata << std::endl;
}

void WriteFile(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 3)
        throw NeedUsage("write-file <parent-folder-id> <file-name> <target-object-line-count>");

    auto folderId = argv.at(0);
    auto fileName = argv.at(1);
    auto lineCount = std::stoi(argv.at(2));

    std::string const text = "Lorem ipsum dolor sit amet";
    auto stream = client->WriteFile(folderId, fileName);

    for (int line = 0; line < lineCount; ++line) stream << (line + 1) << ": " << text << std::endl;

    stream.Close();
    auto meta = std::move(stream).GetMetadata();
    if (!meta)
        throw std::runtime_error(meta.GetStatus().Message());

    std::cout << "Successfully wrote to file " << meta->GetName() << " size is: " << meta->GetSize()
              << " Metadata: " << *meta << std::endl;
}

void WriteLargeFile(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 3)
        throw NeedUsage("write-large-file <parent-folder-id> <file-name> <size-in-MiB>");

    auto folderId = argv.at(0);
    auto fileName = argv.at(1);
    auto sizeMiB = std::stoi(argv.at(2));

    // We want random-looking data, but we do not care if the data has a lot of
    // entropy, so do not bother with a complex initialization of the PRNG seed.
    std::mt19937_64 gen;
    auto generateLine = [&gen]() -> std::string {
        std::string const chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ012345789";
        std::string line(128, '\n');
        std::uniform_int_distribution<std::size_t> uni(0, chars.size() - 1);
        std::generate_n(line.begin(), 127, [&] { return chars.at(uni(gen)); });
        return line;
    };

    // Each line is 128 bytes, so the number of lines is:
    long const MiB = 1024L * 1024;
    long const lineCount = sizeMiB * MiB / 128;

    auto stream = client->WriteFile(folderId, fileName);
    std::generate_n(std::ostream_iterator<std::string>(stream), lineCount, generateLine);

    stream.Close();
    auto meta = std::move(stream).GetMetadata();
    if (!meta)
        throw std::runtime_error(meta.GetStatus().Message());

    std::cout << "Successfully wrote to file " << meta->GetName() << " size is: " << meta->GetSize()
              << " Metadata: " << *meta << std::endl;
}

std::string StartResumableUpload(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 2)
        throw NeedUsage("start-resumable-upload <parent-folder-id> <file-name>");

    auto folderId = argv.at(0);
    auto fileName = argv.at(1);

    auto stream = client->WriteFile(folderId, fileName, csa::NewResumableUploadSession(), csa::AutoFinalizeDisabled());
    auto sessionId = stream.GetResumableSessionId();
    std::cout << "Created resumable upload: " << sessionId << std::endl;
    // Because this stream was created with `AutoFinalizeDisabled()` its
    // destructor will *not* finalize the upload, allowing a separate process or
    // function to resume and continue the upload.
    stream << "This data will not get uploaded, it is too small\n";
    return sessionId;
}

void ResumeResumableUpload(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 3)
        throw NeedUsage("resume-resumable-upload <parent-folder-id> <file-name> <session-id>");

    auto folderId = argv.at(0);
    auto fileName = argv.at(1);
    auto sessionId = argv.at(2);

    auto stream = client->WriteFile(folderId, fileName, csa::RestoreResumableUploadSession(sessionId));
    if (!stream.IsOpen() && stream.GetMetadata().Ok())
    {
        std::cout << "The upload has already been finalized. The object "
                  << "metadata is: " << *stream.GetMetadata() << "\n";
    }
    if (stream.GetNextExpectedByte() == 0)
    {
        // In this example we create a small object, smaller than the resumable
        // upload quantum (256 KiB), so either all the data is there or not.
        // Applications use `next_expected_byte()` to find the position in their
        // input where they need to start uploading.
        stream << R"""(
Lorem ipsum dolor sit amet, consectetur adipiscing
elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit
esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
)""";
    }

    stream.Close();

    auto meta = stream.GetMetadata();
    if (!meta)
    {
        throw std::runtime_error(meta.GetStatus().Message());
    }

    std::cout << "Upload completed, the new object metadata is: " << *meta << std::endl;
}

std::string SuspendResumableUpload(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 2)
        throw NeedUsage("suspend-resumable-upload <parent-folder-id> <file-name>");

    auto folderId = argv.at(0);
    auto fileName = argv.at(1);

    auto stream = client->WriteFile(folderId, fileName, csa::NewResumableUploadSession());
    auto sessionId = stream.GetResumableSessionId();
    std::cout << "Created resumable upload: " << sessionId << "\n";
    // As it is customary in C++, the destructor automatically closes the
    // stream, that would finish the upload and create the object. For this
    // example we want to restore the session as-if the application had crashed,
    // where no destructors get called.
    stream << "This data will not get uploaded, it is too small" << std::endl;
    std::move(stream).Suspend();
    return sessionId;
}

void DeleteResumableUpload(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 2)
        throw NeedUsage("delete-resumable-upload <parent-folder-id> <file-name>");

    auto folderId = argv.at(0);
    auto fileName = argv.at(1);
    auto stream = client->WriteFile(folderId, fileName, csa::NewResumableUploadSession());
    std::cout << "Created resumable upload: " << stream.GetResumableSessionId() << "\n";

    auto status = client->DeleteResumableUpload(stream.GetResumableSessionId());
    if (!status.Ok())
        throw std::runtime_error(status.Message());
    std::cout << "Deleted resumable upload: " << stream.GetResumableSessionId() << std::endl;

    stream.Close();
}

void DownloadFile(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 2)
        throw NeedUsage{"download-file <file-id> <destination-file-name>"};

    auto fileId = argv.at(0);
    auto dstFileName = argv.at(1);

    auto status = client->DownloadFile(fileId, dstFileName);

    if (!status.Ok())
    {
        throw std::runtime_error(status.Message());
    }

    std::cout << "Downloaded file \"" << fileId << "\" to " << dstFileName << std::endl;
}

void ReadFile(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 1)
        throw NeedUsage{"read-file <file-id>"};

    auto fileId = argv.at(0);

    auto stream = client->ReadFile(fileId);
    int lineCount = 0;
    std::string line;
    while (std::getline(stream, line, '\n')) ++lineCount;

    std::cout << "The file \"" << fileId << "\" has " << lineCount << " lines." << std::endl;
}

void ReadFileRange(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 3)
        throw NeedUsage{"read-file-range <file-id> <start> <end>"};

    auto fileId = argv.at(0);
    auto start = std::stoll(argv.at(1));
    auto end = std::stoll(argv.at(2));

    auto stream = client->ReadFile(fileId, csa::ReadRange(start, end));
    int lineCount = 0;
    std::string line;
    while (std::getline(stream, line, '\n')) ++lineCount;

    std::cout << "The requested range of file \"" << fileId << "\" has " << lineCount << " lines." << std::endl;
}

void CopyFile(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 3)
        throw NeedUsage{"copy-file <file-id> <destination-parent-folder-id> <destination-name>"};

    auto fileId = argv.at(0);
    auto dstParentId = argv.at(1);
    auto dstFileName = argv.at(2);

    auto fileMeta = client->CopyFile(fileId, dstParentId, dstFileName);
    if (!fileMeta)
        throw std::runtime_error(fileMeta.GetStatus().Message());

    std::cout << "Successfully copied \"" << fileId << "\" to " << dstFileName << ", full metadata: " << *fileMeta
              << std::endl;
}

void CreateFolder(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || argv.size() != 2)
        throw NeedUsage{"create-folder <parent-id> <name>"};

    auto parentId = argv.at(0);
    auto name = argv.at(1);

    auto folderMeta = client->CreateFolder(parentId, name);
    if (!folderMeta)
        throw std::runtime_error(folderMeta.GetStatus().Message());

    std::cout << "Successfully created folder \"" << name
              << "\", "
                 "full metadata: "
              << *folderMeta << std::endl;
}

void GetQuota(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || !argv.empty())
        throw NeedUsage{"get-quota"};

    auto quota = client->GetQuota();
    if (!quota)
        throw std::runtime_error(quota.GetStatus().Message());

    std::cout << "Storage quota (bytes): " << *quota << std::endl;
}

void GetUserInfo(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || !argv.empty())
        throw NeedUsage{"get-user-info"};

    auto info = client->GetUserInfo();
    if (!info)
        throw std::runtime_error(info.GetStatus().Message());

    std::cout << "User info: " << *info << std::endl;
}

void RunAll(csa::CloudStorageClient* client, std::vector<std::string> const& argv)
{
    if (!client || !argv.empty())
        throw NeedUsage{"auto"};

    auto generator = csa::internal::DefaultPRNG(std::random_device{}());
    auto const example_folder_name = MakeRandomFolderName(generator);

    std::cout << "\nCreating folder to run the example (" << example_folder_name << ")" << std::endl;
    auto root_meta = client->GetFolderMetadata("root");
    if (!root_meta)
        throw std::runtime_error(root_meta.GetStatus().Message());
    auto example_folder_meta = client->CreateFolder(root_meta->GetCloudId(), example_folder_name);
    auto const example_folder_id = example_folder_meta->GetCloudId();

    auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::string const file_media("a-string-to-serve-as-file-media");
    auto const file_name = MakeRandomObjectName(generator, "file-");

    std::cout << "\nRunning InsertFile() example" << std::endl;
    InsertFile(client, {example_folder_id, file_name, file_media});

    std::cout << "\nRunning ListFolder() example" << std::endl;
    ListFolder(client, {example_folder_id});

    std::cout << "\nRunning ListFolderWithPageSize() example" << std::endl;
    ListFolderWithPageSize(client, {example_folder_id, "2"});

    std::cout << "\nRunning CreateFolder() example" << std::endl;
    CreateFolder(client, {example_folder_id, "TestFolder"});

    auto testFolderId = GetObjectId(client, example_folder_id, "TestFolder", true);
    std::cout << "\nRunning GetFolderMetadata() example" << std::endl;
    GetFolderMetadata(client, {testFolderId});

    std::cout << "\nRunning RenameFolder() example [1]" << std::endl;
    RenameFolder(client, {testFolderId, "TestFolder1"});

    // Renaming and moving folder in one operation.
    std::cout << "\nRunning RenameFolder() example [2]" << std::endl;
    auto another_folder_meta = client->CreateFolder(root_meta->GetCloudId(), "AnotherTestFolder");
    auto const another_folder_id = another_folder_meta->GetCloudId();
    RenameFolder(client, {testFolderId, "TestFolder3", example_folder_id, another_folder_id});

    std::cout << "\nRunning PatchDeleteFolderMetadata() example" << std::endl;
    PatchDeleteFolderMetadata(client, {testFolderId, "modifiedTime"});

    std::cout << "\nRunning GetFileMetadata() example" << std::endl;
    auto object_id = GetObjectId(client, example_folder_id, file_name, false);
    GetFileMetadata(client, {object_id});

    std::cout << "\nRunning PatchDeleteFileMetadata() example" << std::endl;
    auto file_id = GetObjectId(client, example_folder_id, file_name, false);
    PatchDeleteFileMetadata(client, {file_id, "modifiedTime"});

    std::cout << "\nRunning RenameFile() example[1]" << std::endl;
    std::string file_name_rename1 = file_name + "_rename_1";
    RenameFile(client, {file_id, file_name_rename1});

    std::cout << "\nRunning RenameFile() example[2]" << std::endl;
    std::string file_name_rename2 = file_name + "_rename_2";
    RenameFile(client, {file_id, file_name_rename2, example_folder_id, testFolderId});

    // Uploading
    auto const filename_1 = MakeRandomFilename(generator);
    auto const file_name_1_cloud = MakeRandomObjectName(generator, "file-");
    std::string const text = R"""(
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor
incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis
nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.
Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu
fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
culpa qui officia deserunt mollit anim id est laborum.
)""";

    std::cout << "\nCreating file for upload" << std::endl;
    std::ofstream(filename_1).write(text.data(), text.size());

    std::cout << "\nRunning UploadFile() example" << std::endl;
    UploadFile(client, {filename_1, example_folder_id, file_name_1_cloud});

    std::cout << "\nRunning the DownloadFile() example" << std::endl;
    auto file_name_1_cloud_id = GetObjectId(client, example_folder_id, file_name_1_cloud, false);
    DownloadFile(client, {file_name_1_cloud_id, filename_1});

    std::cout << "\nDeleting uploaded file" << std::endl;
    (void)client->Delete(file_name_1_cloud_id);

    std::cout << "\nCreating file for upload" << std::endl;
    std::ofstream(filename_1).write(text.data(), text.size());

    std::cout << "\nRunning the UploadFileResumable() example" << std::endl;
    UploadFileResumable(client, {filename_1, example_folder_id, file_name_1_cloud});

    std::cout << "\nDeleting uploaded object" << std::endl;
    file_name_1_cloud_id = GetObjectId(client, example_folder_id, file_name_1_cloud, false);
    (void)client->Delete(file_name_1_cloud_id);

    std::cout << "\nRemoving local file" << std::endl;
    std::remove(filename_1.c_str());

    // End uploading

    // Resumable uploading

    auto const file_name_resumable_upload = MakeRandomObjectName(generator, "file-resumable-upload-");
    std::cout << "\nRunning StartResumableUpload() example" << std::endl;
    auto const start_id = StartResumableUpload(client, {example_folder_id, file_name_resumable_upload});

    std::cout << "\nRunning ResumeResumableUpload() example [1]" << std::endl;
    ResumeResumableUpload(client, {example_folder_id, file_name_resumable_upload, start_id});

    std::cout << "\nRunning SuspendResumableUpload() example" << std::endl;
    auto const suspend_id = SuspendResumableUpload(client, {example_folder_id, file_name_resumable_upload});

    std::cout << "\nRunning ResumeResumableUpload() example [2]" << std::endl;
    ResumeResumableUpload(client, {example_folder_id, file_name_resumable_upload, suspend_id});

    std::cout << "\nRunning DeleteResumableUpload() example" << std::endl;
    DeleteResumableUpload(client, {example_folder_id, file_name_resumable_upload});

    auto file_name_resumable_upload_id = GetObjectId(client, example_folder_id, file_name_resumable_upload, false);
    (void)client->Delete(file_name_resumable_upload_id);

    // End resumable uploading

    std::cout << "\nRunning WriteFile() example" << std::endl;
    WriteFile(client, {example_folder_id, file_name, "100000"});

    std::cout << "\nRunning WriteLargeFile() example" << std::endl;
    WriteLargeFile(client, {example_folder_id, file_name, "10"});

    std::cout << "\nRunning ReadFile() example" << std::endl;
    auto object_name_id = GetObjectId(client, example_folder_id, file_name, false);
    ReadFile(client, {object_name_id});

    std::cout << "\nRunning ReadFileRange() example" << std::endl;
    ReadFileRange(client, {object_name_id, "1000", "2000"});

    std::cout << "\nRunning CopyFile() example" << std::endl;
    auto const copied_object_name = MakeRandomObjectName(generator, "copied-object-");
    CopyFile(client, {object_name_id, example_folder_id, copied_object_name});
    auto const copied_object_name_id = GetObjectId(client, example_folder_id, copied_object_name, false);
    Delete(client, {copied_object_name_id});

    std::cout << "\nRunning Delete() example [1]" << std::endl;
    Delete(client, {file_id});

    std::cout << "\nRunning GetQuota() example" << std::endl;
    GetQuota(client, {});

    std::cout << "\nRunning GetQuota() example" << std::endl;
    GetUserInfo(client, {});

    std::this_thread::sleep_until(pause);
    (void)RemoveFolderAndContents(client, example_folder_id);
}

}  // namespace

int main(int argc, char* argv[])
try
{
    using CmdFn = std::function<void(csa::CloudStorageClient*, std::vector<std::string> const&)>;
    std::map<std::string, CmdFn> cmdMap = {
        {"delete", &Delete},
        {"list-folder", &ListFolder},
        {"list-folder-with-page-size", &ListFolderWithPageSize},
        {"create-folder", CreateFolder},
        {"get-folder-metadata", &GetFolderMetadata},
        {"rename-folder", &RenameFolder},
        {"patch-delete-folder-metadata", &PatchDeleteFolderMetadata},
        {"get-file-metadata", &GetFileMetadata},
        {"patch-delete-file-metadata", &PatchDeleteFileMetadata},
        {"rename-file", &RenameFile},
        {"insert-file", &InsertFile},
        {"upload-file", &UploadFile},
        {"upload-file-resumable", &UploadFileResumable},
        {"write-file", &WriteFile},
        {"write-large-file", &WriteLargeFile},
        {"start-resumable-upload", &StartResumableUpload},
        {"resume-resumable-upload", &ResumeResumableUpload},
        {"suspend-resumable-upload", &SuspendResumableUpload},
        {"delete-resumable-upload", &DeleteResumableUpload},
        {"download-file", &DownloadFile},
        {"read-file", &ReadFile},
        {"read-file-range", &ReadFileRange},
        {"copy-file", CopyFile},
        {"get-quota", GetQuota},
        {"get-user-info", GetUserInfo},
        {"auto", RunAll},
    };
    for (auto&& cmd : cmdMap)
    {
        try
        {
            cmd.second(nullptr, {});
        }
        catch (NeedUsage const& ex)
        {
            commandUsage += "\t";
            commandUsage += ex.what();
            commandUsage += "\n";
        }
    }

    if (argc < 3)
    {
        throw NeedUsage("Missing provider name and/or command.");
    }

    std::string provider = argv[1];
    auto provider_it = std::find_if(csa::ProviderNames.begin(), csa::ProviderNames.end(),
                                    [&provider](auto& item) { return item.second == provider; });
    if (provider_it == std::end(csa::ProviderNames))
    {
        std::cerr << "Unknown provider: " << provider << std::endl;
        return 1;
    }
    // Create a cloud storage client.
    csa::StatusOrVal<csa::CloudStorageClient> client =
        csa::CloudStorageClient(csa::Options{}.Set<csa::ProviderOption>(csa::EProvider::GoogleDrive));
    if (!client)
    {
        std::cerr << "Failed to create cloud storage client, status=" << client.GetStatus() << std::endl;
        return 1;
    }

    std::string cmd = argv[2];
    auto cmdIt = cmdMap.find(cmd);
    if (cmdIt == std::end(cmdMap))
    {
        throw NeedUsage("Unknown command: " + cmd);
    }

    cmdIt->second(&*client, {argv + 3, argv + argc});

    return 0;
}
catch (NeedUsage const& ex)
{
    PrintUsage(argc, argv, ex.what());
    return 0;
}
catch (std::exception const& ex)
{
    std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
    return 1;
}
