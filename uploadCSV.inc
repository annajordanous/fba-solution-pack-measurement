<?php

function fba_solution_pack_uploadCSV(&$form_state = array()) {

  $form = array();
// Set the form encoding type.
  $form['#attributes']['enctype'] = "multipart/form-data"; 

// TODO Set a maximum file size (CHECK THIS)
/*	$form['hidden'] = array(
		'#type' => 'hidden',
		'#name' => 'MAX_FILE_SIZE',
		'#value' =>	'100000',
	); */

// Add a file upload file.
    $form['uploaded_file'] = array(
		'#name' => 'uploaded_file',
		'#type' => 'file',
		'#description' => t('To validate a CSV file of data'),
    );
// Add a submit button
	$form['submit'] = array(
		'#type' => 'submit',
		'#value' => t('Upload file'),
	);
  
   return $form;
}

/**
 * Implements hook_submit.
 *
 * @param array $form
 *   standard form variable for a hook
 * @param array $form_state
 *   standard form state variable for a hook
 */
function fba_solution_pack_uploadCSV_submit($form, &$form_state) {

    if (!$form_state['form_has_errors']) {
    exit();
  }
}


/**
 * Implements form validation.
 *
 * @param array $form
 *   standard form variable for a hook
 * @param array $form_state
 *   standard form state variable for a hook
 */
function fba_solution_pack_uploadCSV_validate($form, &$form_state) {
     $valid_CSV = true;
    // Check file was uploaded successfully as a result of a user upload
    if (!is_uploaded_file($_FILES['uploaded_file']['tmp_name']))  {
        echo "failed - File not uploaded <br />";
		$valid_CSV = false;
    } else {
        echo "File uploaded successfully<br />";

        // Check file is a CSV file
        // Originally tried the line below but doesn't work for CSV files generated
		// 	through Excel's save as CSV function
		// if (strcmp($_FILES['uploaded_file']['type'],'text/csv') <>0) {
		
		if (strcasecmp(end(explode('.', $_FILES['uploaded_file']['name'])),'csv')<>0) {
            echo "This file is not a csv file, file extension is:".end(explode('.', 
$_FILES['uploaded_file']['name']));
			$valid_CSV = false;
		} else {
            // File is a CSV, can start to process it

			// Set files to form_state, to process when form is submitted.
      		$form_state['values']['file'] = $file;

            $file = file_get_contents($_FILES['uploaded_file']['tmp_name']);
			
			// ignore the first row for now - contains non-DTC-vocab headers
			// Just convert it into an array containing each header
            $headers = explode(",", strtok($file, "\n"));
			// Examine the second row and check if each entry in this row  is a DTC vocab term
            $vocab_terms = explode(",", strtok("\n"));

			// NB TODO replace the use of explode() with fgetcsv (in case cells have commas in them)
			
            for ($i=0, $j=count($vocab_terms); $i<$j; $i++)  {
                $term = trim($vocab_terms[$i]);
				fba_solution_pack_uploadCSV_check_vocab_term(&$term, &$valid_CSV);
				// TODO need to collect PIDS and pass these on instead of the terms
            }
		}
	}
	// TODO check all rows have the same number of entries (Is this appropriate?)
	if ($valid_CSV) { 
		echo "<br />File validated successfully<br /><br />"; 
		// File is valid - we can now extract the data from the CSV file and store as RDF
		// Extract the third row information into an array
		$units = explode(",", strtok("\n"));

		// go through and extract the data into an array of arrays
		$current_row = explode(",", strtok("\n"));
		$data_array = array();
		while (count($current_row)>1 && $current_row[0] !== NULL) {
		// add new data to the data array
		$data_array[] = $current_row;
			$current_row = explode(",", strtok("\n"));
		} ;
	fba_solution_pack_uploadCSV_create_RELS($headers, $vocab_terms, $units, $data_array);
	echo "<br />";
	} else {
		echo "<br />File was NOT validated successfully<br /><br />";
	}
}

/**
 * Checks that each term in the vocab row of a CSV file starts with the right prefix and is a valid vocab term
 *
 * @param array $term
 *   the term being checked for validity
 * @param array $valid_CSV
 *   standard valid_CSV boolean variable for whether the CSV file has been found valid or not
 */
function fba_solution_pack_uploadCSV_check_vocab_term(&$term, &$valid_CSV)  {
	// check that each term in the vocab row starts with the right prefix
	if (!ereg("^dtc-vocab:", $term))   {
		// if a term does not start dtc-vocab, reject
		$valid_CSV = false;                     
		echo "$term is not a vocabulary term (missing the dtc-vocab: prefix) <br />";
	} else {
		$term=trim(substr(strstr($term, ":"), 1), " \"");
		// Check that each term in the vocab row is a valid vocab term	
		$query = 'SELECT ?s WHERE {
				?s <info:fedora/fedora-system:def/model#label> "'.$term.'" .
				?s <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> 
					<info:fedora/vocab:collection> .
			}'; 
		// run the query to see if current term is a vocab term. 
		// NB $limit is set to 1 as we only want to check whether $term exists in the vocabulary or not
		// TUPLE query as we aren't interested in retrieving whole triples
		$result = uploadCSV_risearch($query, 'TUPLE', 1, 'Sparql'); 
		$returnedURIs = fba_solution_pack_uploadCSV_get_URI_from_Sparql_result($result);
		if (count($returnedURIs) <>1) {
			// If at first the term isn't found, this could be because it 
			//   isn't capitalised the same way as in the Vocabulary.
			//   Usually vocab terms have First letter only capitalised
			//   so try converting the term to the same form and looking for that
			$ucfirstterm = ucfirst(strtolower($term));
			if (strcmp($term, $ucfirstterm) <>0)  { 
				$query = 'SELECT ?s WHERE {
					?s <info:fedora/fedora-system:def/model#label> "'.$ucfirstterm.'" .
					?s <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> 
						<info:fedora/vocab:collection> .
				}'; 
				echo "Not finding \"$term\", trying it with capitalisation as $ucfirstterm. ";
				// run the query to see if current term is a vocab term. 
				$result = uploadCSV_risearch($query, 'TUPLE', 1, 'Sparql'); 
				$returnedURIs = fba_solution_pack_uploadCSV_get_URI_from_Sparql_result($result);
			}
		if (count($returnedURIs) <>1) {
				// If the term still isn't being found, try doing a case-insensitive 
				//  filtered search for the term. This is much slower than the above searches.
				//  TODO replace this search by a Solr search?						
				$query = 'SELECT ?s WHERE {
					?s <info:fedora/fedora-system:def/model#label> ?label .
					?s <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> 
						<info:fedora/vocab:collection> .
					FILTER (regex(str(?label), "'.$term.'", -i)); 
				}'; 
				echo "Not finding \"$term\", trying a case insensitive search (slower). ";
				// run the query to see if current term is a vocab term. 
				$result = uploadCSV_risearch($query, 'TUPLE', 1, 'Sparql'); 
				if ($result == 0) {
					// TODO search for partial match? Possibly Solr
					// TODO but for now the search stops here
					echo "<br />$term is invalid vocab term<br />";
					$valid_CSV = false;
				} else {
					echo "<br />Case insensitive search for $term validated successfully<br />";
					echo "To speed up the validation process, make sure you capitalise the 
							DTC vocab terms as they are capitalised in the CVocabulary.<br />";
				}
			} else {
				echo "<br />$ucfirstterm validated successfully<br />";
			}
		} else {
			echo "<br />$term validated successfully<br />";
		}
	}
	// TODO need to retrieve and return PID for vocab term and use this
	// TODO NB currently I'm using the term as in the CSV even if it is capitalised wrongly
	
}

function fba_solution_pack_uploadCSV_get_URI_from_Sparql_result($result)  {
		$sxl_result = simplexml_load_string($result);
		$returnedURIs = array();
		foreach ($sxl_result->results->children() as $things) {
			$returnedURIs[] = $things->s["uri"];
      }
      return $returnedURIs;
}

function fba_solution_pack_uploadCSV_create_RELS($headers, $vocab_terms, $units, $data_array)  {
// TODO
echo "<br /><br/><b>Headers:</b> ";
print_r($headers);
echo "<br /><br /><b>Vocab Terms:</b> ";
print_r($vocab_terms);
echo "<br /><br /><b>Units:</b> ";
print_r($units);
echo "<br /><br /><b>Data Array:</b> ";
print_r($data_array);
	/** to be returned: 
	<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
	xmlns:rdfs="http://www.w3.org/2000/01/rdf-schema#" 
	xmlns:relsext="info:fedora/fedora-system:def/relations-external#" 
	xmlns:altrelsext="http://www.fedora.info/definitions/1/0/fedora-relsext-ontology.rdfs#"
	xmlns:dc="http://purl.org/dc/elements/1.1/" 
	xmlns:oai_dc="http://www.openarchives.org/OAI/2.0/oai_dc/" 
	xmlns:fedora-model="info:fedora/fedora-system:def/model#"
	xmlns:dtc="http://www.dtcarchive.org/ontology#">
	*/

	$returnedRDF = new SimpleXMLElement('<rdf_RDF/>');
	
	//add namespaces NB there must be a better way TODO this
	returnedRDF->addAttribute('xmlns_rdf',"http://www.w3.org/1999/02/22-rdf-syntax-ns#");
	returnedRDF->addAttribute('xmlns_rdfs', "http://www.w3.org/2000/01/rdf-schema#");
	returnedRDF->addAttribute('xmlns_relsext', "info:fedora/fedora-system:def/relations-external#");
	returnedRDF->addAttribute('xmlns_altrelsext', "http://www.fedora.info/definitions/1/0/fedora-relsext-ontology.rdfs#");
	returnedRDF->addAttribute('xmlns_dc', "http://purl.org/dc/elements/1.1/");
	returnedRDF->addAttribute('xmlns_oai_dc', "http://www.openarchives.org/OAI/2.0/oai_dc/");
	returnedRDF->addAttribute('xmlns_fedora-model', "info:fedora/fedora-system:def/model#");
	returnedRDF->addAttribute('xmlns_dtc', "http://www.dtcarchive.org/ontology#");
	
    // TODO populate with data		
	$desc = $returnedRDF->addChild('rdf_Description');
	$constituent = $desc->addChild('relsext_hasConstituent');
	$constituent->addAttribute('rdf_parseType', "Resource");

	$constituent->addChild('dc_identifier', "1");

	$constituent->addChild('relsext_HasDescription', "Year");
	
	//need better predicate
	$pointerToVocab = 
		$constituent->addChild('dtc_containsDataOnVocabTerm');
	$pointerToVocab->addAttribute('rdf_resource',"info:fedora/vocab:1111");
	
	//need better predicate	
	$constituent->addChild('dtc_hasPreferredUnits', "N/A");
	
	$subset = $constituent->addChild('relsext_hasSubset');
	$seq = $subset->addChild('rdf_Seq');

	$seq->addChild('rdf_li',"2007");
	$seq->addChild('rdf_li',"2008");
	$seq->addChild('rdf_li',"2009");
	$seq->addChild('rdf_li',"2010");


/* Using 
http://stackoverflow.com/questions/486757/how-to-generate-xml-file-dynamically-using-php
*/
echo "<br />ReturnedRDF <br />";
	print(htmlspecialchars($returnedRDF->asXML()));

}


/**
 * First attempt at generic risearch function.
 *
 * @param string $risearchstring
 *   actual string to be passed to risearch
 * @param string $type
 *   TUPLE (default) or TRIPLE
 * @param int $limit
 *   limit on number of results to return (default 1000) 
 * @param string $format
 *   Return format to return - see risearch documentation (no default set)
 *   TODO Set default
 *
 * @return string
 *   raw search return values
 */
function uploadCSV_risearch($risearchstring, $type = 'TUPLE', $limit=1000, $format) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
//  $offset = 0;
//  $limit = 1000;
  // default value for $format, if not set, is N-Triples for $type='TRIPLE
  $encodedquery = htmlentities(urlencode($risearchstring));

  $url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
  if ($type == 'TRIPLE') {
    $url .= "?type=triples&flush=TRUE&format=$format&limit=$limit&lang=sparql&stream=on&query=" . $encodedquery;
  }
  else {
    $url .= "?type=tuples&flush=TRUE&format=$format&limit=$limit&lang=sparql&stream=on&query=" . $encodedquery;
  }
  $content = trim(do_curl($url));
  return $content != '' ? $content : NULL;
}

