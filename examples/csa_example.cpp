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
#include <functional>

namespace {
class NeedUsage : public std::exception
{
public:
    NeedUsage() : std::exception() {}
    NeedUsage(const char* const msg) : std::exception(msg) {}
    NeedUsage(std::string const& msg) : std::exception(msg.c_str()) {}
};

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
        << commandUsage << "\n";
}

char const* ConsumeArg(int& argc, char* argv[])
{
    if (argc < 2)
    {
        return nullptr;
    }

    char const* result = argv[1];
    std::copy(argv + 2, argv + argc, argv + 1);
    argc--;
    return result;
}

void Delete(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 2)
        throw NeedUsage("delete <object-id>");

    auto objectId = ConsumeArg(argc, argv);
    auto status = client->Delete(objectId);
    if (status.Ok())
        std::cout << "Successfully deleted object " << objectId;
    else
        std::cout << "Faild to delete object " << objectId << status;
}

void ListFolder(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 2)
        throw NeedUsage("list-folder <folder-id>");

    auto folderName = ConsumeArg(argc, argv);
    for (auto&& metadata : client->ListFolder(folderName))
    {
        if (!metadata)
            throw std::runtime_error(metadata.GetStatus().Message());

        std::visit([](auto&& item) {
            std::cout << item.GetName() << std::endl <<
            "\t" << item << std::endl << std::endl; }, *metadata);
    }
}

void ListFolderWithPageSize(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 3)
        throw NeedUsage("list-folder-with-page-size <folder-path> <page-size>");

    auto folderName = ConsumeArg(argc, argv);
    int pageSize = std::stoi(ConsumeArg(argc, argv));
    for (auto&& metadata : client->ListFolder(folderName, csa::PageSize(pageSize)))
    {
        if (!metadata)
            throw std::runtime_error(metadata.GetStatus().Message());

        std::visit([](auto&& item) {
            std::cout << item.GetName() << std::endl <<
                "\t" << item << std::endl << std::endl; }, *metadata);
    }

}

void GetFolderMetadata(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 2)
        throw NeedUsage("get-folder-metadata <folder-path>");
    
    auto folderName = ConsumeArg(argc, argv);
    csa::StatusOrVal<csa::FolderMetadata> folderMetadata = client->GetFolderMetadata(folderName);
    if (!folderMetadata)
    {
        throw std::runtime_error(folderMetadata.GetStatus().Message());
    }

    std::cout << "\'" << folderMetadata->GetName() << "\' " <<
        *folderMetadata << "\n";
}

void GetFileMetadata(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 2)
        throw NeedUsage("get-file-metadata <file-path>");

    auto fileName = ConsumeArg(argc, argv);
    csa::StatusOrVal<csa::FileMetadata> fileMetadata = client->GetFileMetadata(fileName);
    if (!fileMetadata)
    {
        throw std::runtime_error(fileMetadata.GetStatus().Message());
    }

    std::cout << "\'" << fileMetadata->GetName() << "\' " <<
        *fileMetadata << "\n";
}

void RenameFile(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || (argc != 3 && argc != 5))
        throw NeedUsage("rename-file <file-id> <new-name> [<parent-id> <new-parent-id>]");

    auto fileId = ConsumeArg(argc, argv);
    auto newName = ConsumeArg(argc, argv);
    std::string parentId;
    std::string newParentid;
    if (argc == 3)// total 5, 2 are consumed above.
    {
        parentId = ConsumeArg(argc, argv);
        newParentid = ConsumeArg(argc, argv);
    }
    csa::StatusOrVal<csa::FileMetadata> fileMetadata = client->RenameFile(fileId, newName, parentId, newParentid);
    if (!fileMetadata)
    {
        throw std::runtime_error(fileMetadata.GetStatus().Message());
    }

    std::cout << "Rename file succeeded: id=\'" << fileId << "\' "
        << *fileMetadata << std::endl;
}

void InsertFile(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 4)
        throw NeedUsage("insert-file <parent-folder-id> <file-name> <file-content (string)>");

    auto folderId = ConsumeArg(argc, argv);
    auto fileName = ConsumeArg(argc, argv);
    auto contents = ConsumeArg(argc, argv);

    // It runs multipart insertion if name is not empty.
    auto fileMetadata = client->InsertFile(folderId, fileName, contents);
    if (!fileMetadata)
        throw std::runtime_error(fileMetadata.GetStatus().Message());

    std::cout << "Insert file succeded: id=\'" << fileMetadata->GetCloudId() << "\' "
        << *fileMetadata << std::endl;
}

void UploadFile(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 4)
        throw NeedUsage("upload-file <src-file-name> <parent-folder-id> <file-name>");

    auto srcFileName = ConsumeArg(argc, argv);
    auto folderId = ConsumeArg(argc, argv);
    auto fileName = ConsumeArg(argc, argv);

    auto fileMetadata = client->UploadFile(srcFileName, folderId, fileName);
    if (!fileMetadata)
        throw std::runtime_error(fileMetadata.GetStatus().Message());

    std::cout << "Uploaded file " << srcFileName << " succeded to cloud file: id=\'" << fileMetadata->GetCloudId() << "\' "
        << *fileMetadata << std::endl;
}

void UploadFileResumable(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 4)
        throw NeedUsage("upload-file-resumable <src-file-name> <parent-folder-id> <file-name>");

    auto srcFileName = ConsumeArg(argc, argv);
    auto folderId = ConsumeArg(argc, argv);
    auto fileName = ConsumeArg(argc, argv);

    auto fileMetadata = client->UploadFile(srcFileName, folderId, fileName, csa::NewResumableUploadSession());
    if (!fileMetadata)
        throw std::runtime_error(fileMetadata.GetStatus().Message());

    std::cout << "Uploaded file " << srcFileName << " succeded to cloud file: id=\'" << fileMetadata->GetCloudId() << "\' "
        << *fileMetadata << std::endl;
}

void WriteFile(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 4)
        throw NeedUsage("write-file <parent-folder-id> <file-name> <target-object-line-count>");

    auto folderId = ConsumeArg(argc, argv);
    auto fileName = ConsumeArg(argc, argv);
    auto lineCount = std::stol(ConsumeArg(argc, argv));

    std::string const text = "Lorem ipsum dolor sit amet";
    auto stream = client->WriteFile(folderId, fileName);

    for (int line = 0; line < lineCount; ++line)
        stream << (line + 1) << ": " << text << std::endl;

    stream.Close();
    auto meta = std::move(stream).GetMetadata();
    if (!meta)
        throw std::runtime_error(meta.GetStatus().Message());

    std::cout << "Successfully wrote to file " << meta->GetName()
        << " size is: " << meta->GetSize() <<
        " Metadata: " << *meta << std::endl;
}

void WriteLargeFile(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 4)
        throw NeedUsage("write-large-file <parent-folder-id> <file-name> <size-in-MiB>");

    auto folderId = ConsumeArg(argc, argv);
    auto fileName = ConsumeArg(argc, argv);
    auto sizeMiB = std::stoi(ConsumeArg(argc, argv));

    // We want random-looking data, but we do not care if the data has a lot of
    // entropy, so do not bother with a complex initialization of the PRNG seed.
    std::mt19937_64 gen;
    auto generateLine = [&gen]() -> std::string
    {
        std::string const chars =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ012345789";
        std::string line(128, '\n');
        std::uniform_int_distribution<std::size_t> uni(0, chars.size() - 1);
        std::generate_n(line.begin(), 127, [&] { return chars.at(uni(gen)); });
        return line;
    };

    // Each line is 128 bytes, so the number of lines is:
    long const MiB = 1024L * 1024;
    long const lineCount = sizeMiB * MiB / 128;

    auto stream = client->WriteFile(folderId, fileName);
    std::generate_n(std::ostream_iterator<std::string>(stream), lineCount,
        generateLine);

    //stream.Close();
    // TODO: print success/fail msg
}

void StartResumableUpload(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 3)
        throw NeedUsage("start-resumable-upload <parent-folder-id> <file-name>");

    auto folderId = ConsumeArg(argc, argv);
    auto fileName = ConsumeArg(argc, argv);

    auto stream = client->WriteFile(folderId, fileName);

    std::cout << "Created resumable upload: " << stream.GetResumableSessionId()
        << "\n";
    // As it is customary in C++, the destructor automatically closes the
    // stream, that would finish the upload and create the object. For this
    // example we want to restore the session as-if the application had crashed,
    // where no destructors get called.
    stream << "This data will not get uploaded, it is too small\n";
    std::move(stream).Suspend();
}

void ResumeResumableUpload(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 4)
        throw NeedUsage("resume-resumable-upload <parent-folder-id> <file-name> <session-id>");

    auto folderId = ConsumeArg(argc, argv);
    auto fileName = ConsumeArg(argc, argv);
    auto sessionId = ConsumeArg(argc, argv);

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

    std::cout << "Upload completed, the new object metadata is: " << *meta
        << "\n";
}

void DownloadFile(csa::CloudStorageClient* client, int& argc, char* argv[])
{
    if (!client || argc != 4)
        throw NeedUsage{ "download-file <parent-folder-id> <file-id> <destination-file-name>" };

    auto folderId = ConsumeArg(argc, argv);
    auto fileId = ConsumeArg(argc, argv);
    auto dstFileName = ConsumeArg(argc, argv);

    auto status = client->DownloadFile(folderId, fileId, dstFileName);

    if (!status.Ok())
    {
        throw std::runtime_error(status.Message());
    }

    std::cout << "Downloaded file \"" << fileId << "\" to " << dstFileName;
}
} // namespace

int main(int argc, char* argv[]) try 
{
    using CmdFn = std::function<void(csa::CloudStorageClient*, int&, char*[])>;
    std::map<std::string, CmdFn> cmdMap = {
        {"delete", &Delete},
        {"list-folder", &ListFolder},
        {"list-folder-with-page-size", &ListFolderWithPageSize},
        {"get-folder-metadata", &GetFolderMetadata},
        {"get-file-metadata", &GetFileMetadata},
        {"rename-file", &RenameFile},
        {"insert-file", &InsertFile},
        {"upload-file", &UploadFile},
        {"upload-file-resumable", &UploadFileResumable},
        {"write-file", &WriteFile},
        {"write-large-file", &WriteLargeFile},
        {"start-resumable-upload", &StartResumableUpload},
        {"resume-resumable-upload", &ResumeResumableUpload},
        {"download-file", &DownloadFile},
    };
    for (auto&& cmd : cmdMap)
    {
        try {
            int fakeArgc = 0;
            cmd.second(nullptr, fakeArgc, argv);
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

    std::string provider = ConsumeArg(argc, argv);
    auto provider_it = std::find_if(csa::ProviderNames.begin(), csa::ProviderNames.end(), [&provider](auto& item) {
        return item.second == provider; });
    if (provider_it == std::end(csa::ProviderNames))
    {
        std::cerr << "Unknown provider: " << provider << std::endl;
        return 1;
    }
    // Create a cloud storage client.
    csa::StatusOrVal<csa::CloudStorageClient> client =
        csa::CloudStorageClient::CreateDefaultClient(csa::EProvider::GoogleDrive);
    if (!client)
    {
        std::cerr << "Failed to create cloud storage client, status=" << client.GetStatus()
            << std::endl;
        return 1;
    }

    std::string cmd = ConsumeArg(argc, argv);
    auto cmdIt = cmdMap.find(cmd);
    if (cmdIt == std::end(cmdMap))
    {
        throw NeedUsage("Unknown command: " + cmd);
    }

    cmdIt->second(&*client, argc, argv);

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
