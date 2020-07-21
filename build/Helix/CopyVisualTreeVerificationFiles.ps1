$picturesFolder = [Environment]::GetFolderPath('MyPictures')
Copy-Item $picturesFolder\*.xml $Env:HELIX_WORKITEM_UPLOAD_ROOT
