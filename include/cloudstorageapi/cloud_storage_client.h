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

#pragma once

#include "cloudstorageapi/client_options.h"
#include "cloudstorageapi/file_metadata.h"
#include "cloudstorageapi/file_stream.h"
#include "cloudstorageapi/folder_metadata.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/folder_requests.h"
#include "cloudstorageapi/internal/logging_client.h"
#include "cloudstorageapi/internal/raw_client.h"
#include "cloudstorageapi/internal/retry_client.h"
#include "cloudstorageapi/list_folder_reader.h"
#include "cloudstorageapi/options.h"
#include "cloudstorageapi/retry_policy.h"
#include "cloudstorageapi/status_or_val.h"
#include "cloudstorageapi/storage_quota.h"
#include "cloudstorageapi/upload_options.h"
#include "cloudstorageapi/user_info.h"

namespace csa {

namespace internal {
class RawClient;
}
namespace auth {
class Credentials;
}

/**
 * Cloud storage client.
 *
 * This is a main class to interact with cloud storage. It is generalization of different cloud storages APIs.
 * It provides member functions to invoke the APIs in the cloud storages service. Some functionality
 * might not be supported by all cloud storages. In this case those functions return empty or default result. See
 * documentation for each function for details.
 *
 * @par Performance
 * Creating an object of this type is a relatively low-cost operation.
 * Connections to the service are created on demand. Copy-assignment and
 * copy-construction are also relatively low-cost operations, they should be
 * comparable to copying a few shared pointers. The first request (or any
 * request that requires a new connection) incurs the cost of creating the
 * connection and authenticating with the service. Note that the library may
 * need to perform other bookkeeping operations that may impact performance.
 * For example, access tokens need to be refreshed from time to time, and this
 * may impact the performance of some operations.
 *
 * @par Thread-safety
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Credentials
 * TODO
 *
 * @par Error Handling
 * This class uses `StatusOrVal<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOrVal<T>` contains the error details. If
 * the `Ok()` member function in the `StatusOrVal<T>` returns `true` then it
 * contains the expected result.
 *
 * @par Optional Parameters
 * Most of the member functions in this class can receive optional parameters
 * to modify their behavior.
 * Each function documents the types accepted as optional parameters. These
 * options can be specified in any order. Specifying an option that is not
 * applicable to a member function results in a compile-time error.
 *
 * @par Retry and Backoff
 *
 * The library automatically retries requests that fail with transient errors,
 * and follows the recommended practice (e.g. for google drive
 * https://developers.google.com/drive/api/v3/handle-errors#exponential-backoff)
 * to backoff between retries.
 *
 * The default policies are to continue retrying for up to 15 minutes, and to
 * use truncated (at 5 minutes) exponential backoff, doubling the maximum
 * backoff period between retries.
 *
 * The application can override these policies when constructing objects of this
 * class.
 *
 *
 */
class CloudStorageClient
{
public:
    /**
     * Build a new client.
     *
     * @param opts the configuration parameters for the `CloudStorageClient`.
     *
     * @see #CloudStorageClientOptionList for a list of useful options.
     *
     */
    explicit CloudStorageClient(Options opts = {});

    /**
     * \brief Get the provider name.
     *
     * @return The provider name (ie. "googledrive", "dropbox", ...)
     */
    std::string GetProviderName() const { return m_RawClient->GetProviderName(); }

    /**
     * @return user info like email address and name if available.
     * This may not be present in certain contexts
     * if the user has not made their email address visible to the requester.
     */
    StatusOrVal<UserInfo> GetUserInfo() const { return m_RawClient->GetUserInfo(); }

    // Common operations (folders and files)

    // TODO(if needed): StatusOrVal<bool> Exists(const std::string& path) const;

    /**
     * Delete object (file or folder) by given id.
     * it deletes object permanently and recursively at least for gdrive.
     *
     * TODO: if needed provide 'bool permanent' and 'bool recursive' parameters.
     *       Permanent is only possible for box, gdrive, sharepoint, onedrivebiz and sharefile
     */
    Status Delete(std::string const& id)
    {
        internal::DeleteRequest request(id);
        return m_RawClient->Delete(request).GetStatus();
    }

    // Folder operations

    /**
     * Returns list of objects metadata located in given folder.
     *
     * @param id an id of a object as defined by the provider. Path or name
     *     of the object should not be used as id, except provider explicitly says this.
     *
     * @param options a list of optional query parameters and/or request headers.
     *     Valid types for this operation include `MaxResults`.
     */
    template <typename... Options>
    ListFolderReader ListFolder(std::string const& id, Options&&... options) const
    {
        internal::ListFolderRequest request(id);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        auto client = m_RawClient;
        return internal::MakePaginationRange<ListFolderReader>(
            request, [client](internal::ListFolderRequest const& r) { return client->ListFolder(r); },
            [](internal::ListFolderResponse r) { return std::move(r.m_items); });
    }

    /**
     * Returns folder's metadata.
     *
     * @param folderId an id of a folder as defined by the provider. Path or name
     *  of the folder should not be used as id, except provider explicitly says this.
     */
    StatusOrVal<FolderMetadata> GetFolderMetadata(std::string const& folderId) const
    {
        internal::GetFolderMetadataRequest request(folderId);
        return m_RawClient->GetFolderMetadata(request);
    }

    /**
     * Creates a new folder.
     *
     * @param parentId the id of the parent folder that will contain the new folder. Path or name
     *  of the folder should not be used as id, except provider explicitly says this.
     * @param newName the name of the new folder.
     */
    template <typename... Options>
    StatusOrVal<FolderMetadata> CreateFolder(std::string const& parentId, std::string const& newName)
    {
        internal::CreateFolderRequest request(parentId, newName);
        return m_RawClient->CreateFolder(request);
    }

    /**
     * Renames a folder and returns the resuting folder's metadata. It includes moving under the new parent.
     *
     * @param id an id of a folder as defined by the provider. Path or name
     *     of the object should not be used as id, except provider explicitly says this.
     * @param newName new folder's name
     * @param parentId current parent folder id
     * @param newParentId new parent folder id
     */
    StatusOrVal<FolderMetadata> RenameFolder(std::string const& id, std::string const& newName,
                                             std::string const& parentId, std::string const& newParentId)
    {
        internal::RenameRequest request(id, newName, parentId, newParentId);
        return m_RawClient->RenameFolder(request);
    }

    /**
     * Computes the difference between two FolderMetadata objects and patches a
     * folder based on that difference. This request only changes the subset of
     * the attributes included in the request.
     *
     * This function creates a patch request to change the writeable attributes in
     * @p original to the values in @p updated.  Non-writeable attributes are
     * ignored, and attributes not present in @p updated are removed. Typically
     * this function is used after the application obtained a value with
     * `GetFolderMetadata` and has modified these parameters.
     *
     * @param folderId an id of a folder as defined by the provider. Path or name
     *     of the object should not be used as id, except provider explicitly says this.
     * @param original the initial value of the folder metadata
     * @param updated the updated value for the folder metadata
     *
     */
    StatusOrVal<FolderMetadata> PatchFolderMetadata(std::string const& folderId, FolderMetadata original,
                                                    FolderMetadata updated)
    {
        internal::PatchFolderMetadataRequest request(folderId, std::move(original), std::move(updated));
        return m_RawClient->PatchFolderMetadata(request);
    }

    // File operations

    /**
     * Return file metadata.
     *
     * @param id an id of a file as defined by the provider. Path or name
     *     of the object should not be used as id, except provider explicitly says this.
     */
    StatusOrVal<FileMetadata> GetFileMetadata(std::string const& id) const
    {
        internal::GetFileMetadataRequest request(id);
        return m_RawClient->GetFileMetadata(request);
    }

    /**
     * Patches the file metadata in a cloud storage.
     *
     * This function creates a patch request to change the writeable attributes in
     * @p original to the values in @p updated. Non-writeable attributes are
     * ignored, and attributes not present in @p updated are removed. Typically
     * this function is used after the application obtained a value with
     * `GetObjectMetadata` and has modified these parameters.
     *
     * @param fileId an id of a file as defined by the provider. Path or name
     *     of the object should not be used as id, except provider explicitly says this.
     * @param original the initial value of the object metadata.
     * @param updated the updated value for the object metadata.
     */
    StatusOrVal<FileMetadata> PatchFileMetadata(std::string fileId, FileMetadata original, FileMetadata updated)
    {
        internal::PatchFileMetadataRequest request(fileId, std::move(original), std::move(updated));
        return m_RawClient->PatchFileMetadata(request);
    }

    /**
     * Renames a file. It includes moveing file to another folder.
     *
     * @param id an id of a file as defined by the provider. Path or name
     *     of the object should not be used as id, except provider explicitly says this.
     * @param newName new file's name
     * @param parentId current parent folder id
     * @param newParentId new parent folder id
     *
     */
    StatusOrVal<FileMetadata> RenameFile(std::string const& id, std::string const& newName,
                                         std::string const& parentId = "", std::string const& newParentId = "")
    {
        internal::RenameRequest request(id, newName, parentId, newParentId);
        return m_RawClient->RenameFile(request);
    }

    /**
     * Creates an object given its name and contents.
     *
     * @param folderId parent folder id
     * @param name the name of the file to be created
     * @param content the contents (media) for the new object
     * @param options a list of optinal query parameters and/or request header.
     *      Valid tpes for thei operation include `ContentEncoding`,
     *     `ContentType`, and `WithObjectMetadata`
     */
    template <typename... Options>
    StatusOrVal<FileMetadata> InsertFile(std::string const& folderId, std::string const& name, std::string content,
                                         Options&&... options)
    {
        internal::InsertFileRequest request(folderId, name, std::move(content));
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return m_RawClient->InsertFile(request);
    }

    /**
     *  Uploads given local file to the storage.
     *
     * @note
     * Only regular files are supported. If you need to upload the results of
     * reading a device, Named Pipe, FIFO, or other type of file system object
     * that is **not** a regular file then `WriteObject()` is probably a better
     * alternative.
     *
     * @param srcFileName the name of the file to be uploaded
     * @param parentId parent folder id
     * @param name the name of the new file in the cloud storage
     * @param options a list of optional query parameters and/or request headers.
     *      Valid types for this operation include `ContentEncoding`, `ContentType`,
     *      `UploadFromOffset`, `UploadLimit` and `WithObjectMetadata`.
     */
    template <typename... Options>
    StatusOrVal<FileMetadata> UploadFile(std::string const& srcFileName, std::string const& parentId,
                                         std::string const& name,
                                         // bool overwrite,
                                         Options&&... options)  // Use multipart upload if size > 100MB
    {
        // Determine, at compile time, which version of UploadFileImpl we should
        // call. This needs to be done at compile time because InsertFile
        // does not support (nor should it support) the UseResumableUploadSession
        // option.
        using HasUseResumableUpload = std::disjunction<std::is_same<UseResumableUploadSession, Options>...>;
        return UploadFileImpl(srcFileName, parentId, name, HasUseResumableUpload{}, std::forward<Options>(options)...);
    }

    /**
     * Cancel a resumable upload.
     *
     * @param uploadSessionUrl the url of the upload session. Returned by
     * `ObjectWriteStream::resumable_session_id`.
     * @param options a list of optional query parameters and/or request headers.
     *   Valid types for this operation include `UserProject`.
     *
     * @par Idempotency
     * This operation is always idempotent because it only acts on a specific
     * `upload_session_url`.
     */
    template <typename... Options>
    Status DeleteResumableUpload(std::string const& uploadSessionUrl, Options&&... options)
    {
        internal::DeleteResumableUploadRequest request(uploadSessionUrl);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return m_RawClient->DeleteResumableUpload(request).GetStatus();
    }

    /**
     * Writes contents into a file.
     *
     * This creates a `std::ostream` object to upload contents. The application
     * can use either the regular `operator<<()`, or `std::ostream::write()` to
     * upload data.
     *
     * This function always uses resumable uploads. The
     * application can provide a `#RestoreResumableUploadSession()` option to
     * resume a previously created upload. The returned object has accessors to
     * query the session id and the next byte expected by GCS.
     *
     * @note When resuming uploads it is the application's responsibility to save
     *     the session id to restart the upload later. Likewise, it is the
     *     application's responsibility to query the next expected byte and send
     *     the remaining data without gaps or duplications.
     *
     * For small uploads `InsertFile` is recommended.
     *
     * If the application does not provide a `#RestoreResumableUploadSession()`
     * option, or it provides the `#NewResumableUploadSession()` option then a new
     * resumable upload session is created.
     *
     * @param parentId the id of the parent folder that contains the object.
     * @param name the name of the object to be written.
     * @param options a list of optional query parameters and/or request headers.
     *   Valid types for this operation include `ContentEncoding`, `ContentType`,
     *   `UseResumableUploadSession`, and `WithObjectMetadata`,
     *   `UploadContentLength`, and `AutoFinalize`.
     */
    template <typename... Options>
    FileWriteStream WriteFile(std::string const& parentId, std::string const& name, Options&&... options)
    {
        internal::ResumableUploadRequest request(parentId, name);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return WriteObjectImpl(request);
    }

    /**
     * Downloads a cloud storage file to a local file.
     *
     * @param parentId the id of the folder that contains the file.
     * @param fileId the id of the file to be downloaded.
     * @param dstFileName the name of the destination file that will have the cloud file
     *   media.
     * @param options a list of optional query parameters and/or request headers.
     *     Valid types for this operation include `ReadFromOffset` and `ReadRange`,
     *     and `ReadLast`.
     *
     */
    template <typename... Options>
    Status DownloadFile(std::string const& fileId, std::string const& dstFileName,
                        Options&&... options)  // TODO: figure out if content type param is needed.
                                               // Maybe for downloading google docs.
    {
        internal::ReadFileRangeRequest request(fileId);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return DownloadFileImpl(request, dstFileName);
    }

    /**
     * Reads the contents of a file.
     *
     * Returns an object derived from `std::istream` which can be used to read the
     * contents of the cloud file. The application should check the `badbit` (e.g.
     * by calling `stream.bad()`) on the returned object to detect if there was
     * an error reading from the file. If `badbit` is set, the application can
     * check the `status()` variable to get details about the failure.
     * Applications can also set the exception mask on the returned stream, in
     * which case an exception is thrown if an error is detected.
     *
     * @param fileId the id of the file to be read.
     * @param options a list of optional query parameters and/or request headers.
     *     Valid types for this operation include `ReadFromOffset`, `ReadRange`
     *     and `ReadLast`.
     */
    template <typename... Options>
    FileReadStream ReadFile(std::string const& fileId, Options&&... options)
    {
        struct HasReadRange : public std::disjunction<std::is_same<ReadRange, Options>...>
        {
        };
        struct HasReadFromOffset : public std::disjunction<std::is_same<ReadFromOffset, Options>...>
        {
        };
        struct HasReadLast : public std::disjunction<std::is_same<ReadLast, Options>...>
        {
        };

        struct HasIncompatibleRangeOptions
            : public std::integral_constant<bool,
                                            HasReadLast::value && (HasReadFromOffset::value || HasReadRange::value)>
        {
        };

        static_assert(!HasIncompatibleRangeOptions::value,
                      "Cannot set ReadLast option with either ReadFromOffset or "
                      "ReadRange.");

        internal::ReadFileRangeRequest request(fileId);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return ReadObjectImpl(request);
    }

    /**
     * Copy an existing file.
     *
     * Use `CopyFile` to copy between files in the same location and storage.
     *
     * @param sourceFileId an id of a file to copy.
     * @param destinationParentFolderId an id of the folder that will contain the new file.
     * @param destinationFileName the name of the new file.
     * @param options a list of optional query parameters and/or request headers.
     *      Valid types for this operation include `WithObjectMetadata`
     */
    template <typename... Options>
    StatusOrVal<FileMetadata> CopyFile(std::string const& sourceFileId, std::string const& destinationParentFolderId,
                                       std::string const& destinationFileName, Options&&... options)
    {
        internal::CopyFileRequest request(sourceFileId, destinationParentFolderId, destinationFileName);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return m_RawClient->CopyFileObject(request);
    }

    /**
     *  Returns storage quota.
     */
    StatusOrVal<StorageQuota> GetQuota() { return m_RawClient->GetQuota(); }

private:
    std::shared_ptr<internal::RawClient> m_RawClient;

    CloudStorageClient(std::shared_ptr<internal::RawClient> rawClient) : m_RawClient(std::move(rawClient)) {}

    static std::shared_ptr<internal::RawClient> CreateDefaultRawClient(Options options);
    static std::shared_ptr<internal::RawClient> CreateDefaultRawClient(Options const& options,
                                                                       std::shared_ptr<internal::RawClient> client);

    // The version of UploadFile() where UseResumableUploadSession is one of the
    // options. Note how this does not use InsertFile at all.
    template <typename... Options>
    StatusOrVal<FileMetadata> UploadFileImpl(std::string const& srcFileName, std::string const& parentId,
                                             std::string const& name, std::true_type, Options&&... options)
    {
        internal::ResumableUploadRequest request(parentId, name);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return UploadFileResumable(srcFileName, std::move(request));
    }

    // The version of UploadFile() where UseResumableUploadSession is *not* one of
    // the options. In this case we can use InsertFileRequest because it
    // is safe.
    template <typename... Options>
    StatusOrVal<FileMetadata> UploadFileImpl(std::string const& srcFileName, std::string const& parentId,
                                             std::string const& name, std::false_type, Options&&... options)
    {
        std::size_t fileSize{0};
        if (UseSimpleUpload(srcFileName, fileSize))
        {
            internal::InsertFileRequest request(parentId, name, std::string{});
            request.SetMultipleOptions(std::forward<Options>(options)...);
            return UploadFileSimple(srcFileName, fileSize, request);
        }
        internal::ResumableUploadRequest request(parentId, name);
        request.SetMultipleOptions(std::forward<Options>(options)...);
        return UploadFileResumable(srcFileName, std::move(request));
    }

    bool UseSimpleUpload(std::string const& fileName, std::size_t& size) const;

    StatusOrVal<FileMetadata> UploadFileSimple(std::string const& fileName, std::size_t file_size,
                                               internal::InsertFileRequest request);

    StatusOrVal<FileMetadata> UploadFileResumable(std::string const& fileName,
                                                  internal::ResumableUploadRequest request);

    StatusOrVal<FileMetadata> UploadStreamResumable(std::istream& source,
                                                    internal::ResumableUploadRequest const& request);

    FileWriteStream WriteObjectImpl(internal::ResumableUploadRequest const& request);

    FileReadStream ReadObjectImpl(internal::ReadFileRangeRequest const& request);

    Status DownloadFileImpl(internal::ReadFileRangeRequest const& request, std::string const& dstFileName);
};

}  // namespace csa
