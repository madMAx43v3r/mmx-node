import { exportFile, useQuasar } from "quasar";

export function useExportTable() {
    const $q = useQuasar();

    const wrapCsvValue = (val, formatFn, row) => {
        let formatted = formatFn !== void 0 ? formatFn(val, row) : val;

        formatted = formatted === void 0 || formatted === null ? "" : String(formatted);

        formatted = formatted.split('"').join('""');
        /**
         * Excel accepts \n and \r in strings, but some other CSV parsers do not
         * Uncomment the next two lines to escape new lines
         */
        // .split('\n').join('\\n')
        // .split('\r').join('\\r')

        return `"${formatted}"`;
    };

    const exportTable = (rows, columns, filename = "export.csv") => {
        // naive encoding to csv format
        const content = [columns.map((col) => wrapCsvValue(col.label))]
            .concat(
                rows.map((row) =>
                    columns
                        .map((col) =>
                            wrapCsvValue(
                                typeof col.field === "function"
                                    ? col.field(row)
                                    : row[col.field === void 0 ? col.name : col.field],
                                col.format,
                                row
                            )
                        )
                        .join(",")
                )
            )
            .join("\r\n");

        const status = exportFile(filename, content, "text/csv");

        if (status !== true) {
            $q.notify({
                message: "Browser denied file download...",
                color: "negative",
                icon: "warning",
            });
        }
    };

    return {
        exportTable,
    };
}
