# Images

Images (`:image[label]{id=…}`, the id from the tail of Google's inline object id) are identities, not locations. Another document's image is `src="gdoc:<doc id>/<image id>"`.

| Token placement | Result |
| --- | --- |
| Untouched | Kept as is |
| Moved within its tab or to another tab, copied to another tab, or copied from another document's export | Recreated from a fresh `contentUri` at its exact size; a move then deletes the original |
| Same, but not faithfully recreatable | A copy lands with a warning naming the lost properties; a move refuses without `allow: ["lossyImageMove"]` |
| Removed | Deleted, with no approval or warning |
| `![](https://…)` | Public-URL insert (Google fetches it) |
| `![](path)`, `file://`, `~`, or `http://` | Uploaded: import reads (or downloads) the image, uploads every one through one throwaway converted .docx titled `[gdocsmith upload]`, inserts each from its `contentUri`, then deletes the throwaway. Only after planning and guards pass, so a refused import uploads nothing |

- One resolver (`src/core/lens/images.ts`) decides every image write and holds the fidelity check. Import reads each source document once per import, since `contentUri` expires after about 30 minutes.
- Faithful means no crop, rotation, rendered border, alt title or description, or brightness, contrast or transparency adjustment. `insertInlineImage` can't set any of them. The natural aspect ratio isn't checked: the API doesn't expose it.
- Rejecting a suggested image deletion inserts a plain copy and deletes the original, under the same fidelity rule. Rejecting a suggested image insertion just deletes it.
- Uploads accept PNG, JPEG and GIF (checked by their bytes) up to 50 MB and 25 megapixels. Google re-encodes them and stores at most 2048 px per side (import warns). Alt text can't be set.
- `export` with `imagesDownload: true` saves each Drive-hosted image to `images/<tab id>/<key>.<ext>` in the export folder (`imagesDir`). `contentUri`s are bearer URLs and never written anywhere.
- Drawings, charts, equations and positioned objects can't be recreated; leave their tokens in place. Resizing and layout belong to the Google Docs UI.
