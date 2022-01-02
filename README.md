# cloudstorageapi
Cloud storage client library abstracts provider details and gives uniform C++ API to multiple cloud storages. Inspired by [google-cloud-cpp](https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/storage).

## Supported cloud storages
Storage service | Version
----------------|--------
Google drive | v3

## Supported platforms
* Windows, coming on macOS and Linux
* C++20 compilers
* CMake builds

## Documentation

Detailed header comments in [public `.h`][source-link] files

[source-link]: https://github.com/awkv/cloudstorageapi/tree/master/include/cloudstorageapi

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

```cc
#include "cloudstorageapi/cloud_storage_client.h"
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Missing file name.\n";
        std::cerr << "Usage: quickstart <file-name>\n";
        return 1;
    }
    std::string const file_name = argv[1];

    // Create a client to communicate with Google Drive. This client
    // uses the default configuration for authentication.
    csa::StatusOrVal<csa::CloudStorageClient> client =
        csa::CloudStorageClient(csa::Options{}.Set<csa::ProviderOption>(csa::EProvider::GoogleDrive));
    if (!client)
    {
        std::cerr << "Failed to create cloud storage client, status=" << client.GetStatus() << std::endl;
        return 1;
    }

    auto root_meta = client->GetFolderMetadata("root");
    if (!root_meta)
    {
        std::cerr << "Failed to get root folder metadata: " << root_meta.GetStatus().Message();
    }

    auto writer = client->WriteFile(root_meta->GetCloudId(), file_name);
    writer << "Hello World!";
    writer.Close();
    if (writer.GetMetadata())
    {
        std::cout << "Successfully created object: " << *writer.GetMetadata() << "\n";
    }
    else
    {
        std::cerr << "Error creating object: " << writer.GetMetadata().GetStatus() << "\n";
        return 1;
    }

    auto file_id = writer.GetMetadata().Value().GetCloudId();
    auto reader = client->ReadFile(file_id);
    if (!reader)
    {
        std::cerr << "Error reading object: " << reader.GetStatus() << "\n";
        return 1;
    }

    std::string contents{std::istreambuf_iterator<char>{reader}, {}};
    std::cout << contents << "\n";

    return 0;
}
````

## Dependencies
* [libcurl](https://curl.se/libcurl/)
* [openssl](https://www.openssl.org/)
* [spdlog](https://github.com/gabime/spdlog)
* [gtest](https://github.com/google/googletest)
* [nlohmann/json](https://github.com/nlohmann/json)

## Building on Windows (static library)

Prerequisites:
- [CMake](https://cmake.org/)
- [Git](https://git-scm.com/downloads)
- [vcpkg](https://github.com/microsoft/vcpkg)
    ```cmd
    > git clone https://github.com/microsoft/vcpkg
    > .\vcpkg\bootstrap-vcpkg.bat
    ```
- [Visual Studio](https://visualstudio.microsoft.com/) 2019 or greater

Building instructions:
```cmd
> [path to vcpkg] install curl gtest nlohmann-json openssl spdlog --triplet=x64-windows-static-md
> git clone --recursive https://github.com/awkv/cloudstorageapi.git
> mkdir cloudstorageapi\build
> cd cloudstorageapi\build
> cmake -G "Visual Studio 16 2019" -A x64 -S .. -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
> cmake --build .
```

Binaries can be found in `cloudstorageapi\build\binaries`

## Authentication

Usually cloud storages require that your application authenticates with the service before accessing any data. The library supports OAuth 2.0 credentials. You can either use the CSA_CREDENTIALS environment variable or write code to pass the account key to the library.

For example (on Windows)
- via environment variable
    ```cmd
    > set CSA_CREDENTIALS="C:\Users\username\Downloads\account-file.json"
    ```
- using code
    ```cc
    auto cred = csa::auth::CredentialFactory::CreateAuthorizedUserCredentialsFromJsonContents(csa::EProvider::GoogleDrive, <json-credentials-string>, "");
    auto client =         csa::CloudStorageClient(csa::Options{}.Set<csa::ProviderOption>(csa::EProvider::GoogleDrive).Set<Oauth2CredentialsOption>(cred));
    ```

Credentials file or credential string is the following json structure:
```json
{
  "client_id": "<cloud-storage-client_id>",
  "client_secret": "<cloud-storage-client-secret>",
  "refresh_token": "<cloud-storage-refresh-token>",
  "token_type": "Bearer"
}
```

## Licensing

Apache 2.0; see [`LICENSE`](LICENSE) for details.
